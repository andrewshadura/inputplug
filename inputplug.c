#define _BSD_SOURCE
#define _GNU_SOURCE
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#if HAVE_HEADER_IXP_H
#include <ixp.h>
#define IXP_GETOPT "a:f:"
#else
#define IXP_GETOPT
#endif

#if HAVE_HEADER_BSD_LIBUTIL_H
#include <bsd/libutil.h>
#else
#if HAVE_HEADER_LIBUTIL_H
#include <libutil.h>
#else
#ifdef HAVE_PIDFILE
#undef HAVE_PIDFILE
#endif
#endif
#endif

#ifdef HAVE_PIDFILE
#define PIDFILE_GETOPT "p:"
#else
#define PIDFILE_GETOPT
#endif

struct pair {
    int key;
    char * value;
};

#define T(x) {x, #x}
#define T_END {0, NULL}

bool verbose = false;
bool no_act = false;

static const struct pair * map(int key, const struct pair * table, bool strict) {
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

#if HAVE_HEADER_IXP_H
static IxpCFid *fid = NULL;

static void postevent(const char *command, char *const args[])
{
    assert(command != NULL);
    assert(args != NULL);

    if (fid != NULL) {
        ixp_print(fid, "%s %s %s\n", args[1], args[2], args[3]);
    }
}
#endif

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

static char * command = NULL;

static int handle_device(int id, int type, int flags, char *name)
{
    const struct pair * use = map(type, device_types, true);
    const struct pair * change = map(flags, changes, false);

    if (change) {
        char deviceid[strlen(UINT_MAX_STRING) + 1];
        sprintf(deviceid, "%d", id);
        char *const args[] = {
            command,
            change->value,
            deviceid,
            use ? use->value : "",
            name ? name : "",
            NULL
        };
        execute(command, args);
        #if HAVE_HEADER_IXP_H
        postevent(command, args);
        #endif

        return change->key;
    }
    return -1;
}

static void parse_event(XIHierarchyEvent *event)
{
    int i;
    for (i = 0; i < event->num_info; i++)
    {
        int flags = event->info[i].flags;
        int j = 16;
        while (flags && j) {
            int ret;
            j--;
            ret = handle_device(event->info[i].deviceid, event->info[i].use, flags, NULL);
            if (ret == -1) {
                break;
            }
            flags -= ret;
        }
    }
}

#if HAVE_HEADER_IXP_H
static char * getenv_dup(const char * name) {
    const char * value = getenv(name);
    if (value != NULL) {
        return strdup(value);
    } else {
        return NULL;
    }
}
#endif

#ifdef HAVE_PIDFILE
struct pidfh *pfh = NULL;
#endif

static pid_t daemonise(void) {
    pid_t pid;
    switch (pid = fork()) {
        case -1:           /* failure */
#ifdef HAVE_PIDFILE
            pidfile_remove(pfh);
#endif
            exit(EXIT_FAILURE);
        case 0:            /* child */
            break;
        default:           /* parent */
            return pid;
    }

    umask(0);

    if (setsid() < 0) {
        perror("setsid");
    }

    if (chdir("/") < 0) {
        perror("chdir");
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    return 0;
}

int main(int argc, char *argv[])
{
    XIEventMask mask;
    int xi_opcode;
    int event, error;
    int opt;
    bool foreground = false, bootstrap = false;

    #if HAVE_HEADER_IXP_H
    IxpClient *client;

    char * address = getenv_dup("WMII_ADDRESS");
    char * path = strdup("/event");
    #endif
    #ifdef HAVE_PIDFILE
    char * pidfile = NULL;
    #endif

    while (((opt = getopt(argc, argv, IXP_GETOPT "vhnd0c:" PIDFILE_GETOPT)) != -1) ||
           ((opt == -1) && (command == NULL))) {
        switch (opt) {
            case 'v':
                verbose = true;
                break;
            case 'n':
                no_act = true;
                foreground = true;
                break;
            case 'd':
                foreground = true;
                break;
            case '0':
                bootstrap = true;
                break;
            case 'c':
                if (command)
                    free(command);
                command = realpath(optarg, NULL);
                break;
            #if HAVE_HEADER_IXP_H
            case 'a':
                if (address)
                    free(address);
                address = strdup(optarg);
                break;
            case 'f':
                if (path)
                    free(path);
                path = strdup(optarg);
                break;
            #endif
            #ifdef HAVE_PIDFILE
            case 'p':
                if (pidfile)
                    free(pidfile);
                pidfile = strdup(optarg);
                break;
            #endif
            default:
                fprintf(stderr, "Usage: %s "
                #ifdef HAVE_HEADER_IXP_H
                "[-a address] [-f path] "
                #endif
                #ifdef HAVE_PIDFILE
                    "[-p pidfile] "
                #endif
                "[-v] [-n] [-d] -c command-prefix\n", argv[0]);
                exit(opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
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

    XCloseDisplay(display);

    #if HAVE_HEADER_IXP_H
    if (address) {
        if (*address) {
            if (verbose) {
                fprintf(stderr, "Connecting to 9P server at %s.\n", address);
            }
            client = ixp_mount(address);
        } else {
            if (verbose) {
                fprintf(stderr, "Connecting to wmii 9P server.\n");
            }
            client = ixp_nsmount("wmii");
        }
        if (client == NULL) {
            fprintf(stderr, "Failed to connect to 9P server: %s\n", ixp_errbuf());
            free(address);
            address = NULL;
        } else {
            ixp_unmount(client);
        }
    }
    #endif

    #ifdef HAVE_PIDFILE
    if (pidfile) {
        pid_t otherpid;
        pfh = pidfile_open(pidfile, 0600, &otherpid);
        if (pfh == NULL) {
            if (errno == EEXIST) {
                fprintf(stderr, "Already running as %ju.\n", (uintmax_t)otherpid);
                exit(EXIT_FAILURE);
            }
            fprintf(stderr, "Can't open or create pidfile.\n");
        }
    }
    #endif

    if (!foreground) {
        pid_t pid;
        if ((pid = daemonise()) != 0) {
            if (verbose) {
                fprintf(stderr, "Daemonised as %ju.\n", (uintmax_t)pid);
            }
            exit(EXIT_SUCCESS);
        }
        exit(EXIT_SUCCESS);
    }

    #ifdef HAVE_PIDFILE
    if (pidfile && pfh) {
        pidfile_write(pfh);
    }
    #endif

    display = XOpenDisplay(NULL);

    if (bootstrap) {
        XDeviceInfo *info;
        int num_devices;
        info = XListInputDevices(display, &num_devices);
        for (int i=0; i < num_devices; i++) {
            switch (info[i].use) {
            case XIMasterPointer:
            case XIMasterKeyboard:
                handle_device(info[i].id, info[i].use, XIMasterAdded, info[i].name);
                break;
            case XISlavePointer:
            case XISlaveKeyboard:
                handle_device(info[i].id, info[i].use, XISlaveAdded, info[i].name);
                handle_device(info[i].id, info[i].use, XIDeviceEnabled, info[i].name);
                break;
            }
        }
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
#ifdef SA_NOCLDWAIT
        .sa_flags = SA_NOCLDWAIT
#endif
    };
    sigaction(SIGCHLD, &sigchld_action, NULL);

    #if HAVE_HEADER_IXP_H
    if (address) {
        if (*address) {
            client = ixp_mount(address);
        } else {
            client = ixp_nsmount("wmii");
        }
        if (client != NULL) {
            fid = ixp_open(client, path, P9_OWRITE);
        }
    }
    #endif

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
