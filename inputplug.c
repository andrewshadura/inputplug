#define _GNU_SOURCE
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <search.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

struct pair {
    int key;
    char * value;
};

#define T(x) {x, #x}
#define T_END {0, NULL}

bool verbose = false;
bool no_act = false;

const struct pair * map(int key, const struct pair * table, bool strict) {
    if (!table) return NULL;
    while (table->value) {
        if ((!strict && (table->key & key)) || (table->key == key)) {
            return table;
        } else {
            table++;
        }
    }
    return NULL;
}

const struct pair device_types[] = {
    T(XIMasterPointer),
    T(XIMasterKeyboard),
    T(XISlavePointer),
    T(XISlaveKeyboard),
    T(XIFloatingSlave),
    T_END
};

const struct pair changes[] = {
    T(XIMasterAdded),
    T(XIMasterRemoved),
    T(XISlaveAdded),
    T(XISlaveRemoved),
    T(XISlaveAttached),
    T(XISlaveDetached),
    T(XIDeviceEnabled),
    T(XIDeviceDisabled),
    T_END
};

static void execute(const char *command, char *const args[])
{
    assert(command != NULL);
    assert(args != NULL);

    if (verbose || no_act) {
        int i;
        for (i = 0; args[i] != NULL; i++) {
            fprintf(stderr, "%s ", args[i]);
        }
        fputc('\n', stderr);
    }
    if (!no_act) {
        pid_t child;

        fflush(NULL);
        setpgid(0, 0);
        switch (child = fork()) {
            case 0:            /* child */
                execvp(command, args);
                exit(127);
            default:           /* parent */
                break;
        }
    }
}

#define STRINGIFY(x) #x
#define EXPAND_STRINGIFY(x) STRINGIFY(x)
#define UINT_MAX_STRING EXPAND_STRINGIFY(UINT_MAX)

char * command = NULL;

static void parse_event(XIHierarchyEvent *event)
{
    int i;
    for (i = 0; i < event->num_info; i++)
    {
        int flags = event->info[i].flags;
        int j = 16;
        while (flags && j) {
            j--;
            const struct pair * use = map(event->info[i].use, device_types, true);
            const struct pair * change = map(flags, changes, false);

            if (change) {
                char deviceid[strlen(UINT_MAX_STRING) + 1];
                sprintf(deviceid, "%d", event->info[i].deviceid);
                char *const args[] = {
                     command,
                     change->value,
                     deviceid,
                     use ? use->value : "",
                     NULL
                };
                execute(command, args);

                flags -= change->key;
            } else {
                break;
            }
        }
    }
}

char * getenv_dup(const char * name) {
    const char * value = getenv(name);
    if (value != NULL) {
        return strdup(value);
    } else {
        return NULL;
    }
}

int main(int argc, char *argv[])
{
    XIEventMask mask;
    int xi_opcode;
    int event, error;

    int opt;

    #if 0
    char * address = getenv_dup("WMII_ADDRESS");
    #endif

    while ((opt = getopt(argc, argv, /* "a:" */ "vnc:")) != -1) {
        switch (opt) {
            case 'v':
                verbose = true;
                break;
            case 'n':
                no_act = true;
                break;
            case 'c':
                if (command)
                    free(command);
                command = realpath(optarg, NULL);
                break;
            #if 0
            case 'a':
                if (address)
                    free(address);
                address = strdup(optarg);
                break;
            #endif
            default:
                fprintf(stderr, "Usage: %s [-v] [-n] -c command-prefix\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    Display *display;
    display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "Can't open X display.\n");
        return EXIT_FAILURE;
    }

    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
        printf("X Input extension not available.\n");
        goto out;
    }

    mask.deviceid = XIAllDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = calloc(mask.mask_len, sizeof(char));
    XISetMask(mask.mask, XI_HierarchyChanged);
    XISelectEvents(display, DefaultRootWindow(display), &mask, 1);
    XSync(display, False);

    free(mask.mask);

    /* avoid zombies when spawning processes */
    struct sigaction sigchld_action = {
        .sa_handler = SIG_DFL,
        .sa_flags = SA_NOCLDWAIT
    };
    sigaction(SIGCHLD, &sigchld_action, NULL);

    while(1) {
        XEvent ev;
        XGenericEventCookie *cookie = (XGenericEventCookie*)&ev.xcookie;
        XNextEvent(display, (XEvent*)&ev);

        if (XGetEventData(display, cookie)) {
            if (cookie->type == GenericEvent &&
                cookie->extension == xi_opcode &&
                cookie->evtype == XI_HierarchyChanged) {
                    parse_event(cookie->data);
            }
            XFreeEventData(display, cookie);
        }
    }


    XSync(display, False);
out:
    XCloseDisplay(display);

    return EXIT_SUCCESS;
}
