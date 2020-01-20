#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <xcb/xcb.h>
#include <xcb/xinput.h>
#include <xcb/xproto.h>

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
        switch ((child = fork())) {
            case 0:            /* child */
                execvp(command, args);
                exit(127);
            default:           /* parent */
                waitpid(child, NULL, 0);
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

static xcb_screen_t *screen_of_display(xcb_connection_t *conn, int screen_no)
{
    xcb_screen_iterator_t iter;
    const xcb_setup_t *setup;

    setup = xcb_get_setup(conn);

    if (setup) {
        iter = xcb_setup_roots_iterator(setup);
        for (; iter.rem; --screen_no, xcb_screen_next(&iter)) {
            if (screen_no == 0) {
                return iter.data;
            }
        }
    }
    return NULL;
}

static char *get_device_name(xcb_connection_t *conn, xcb_input_device_id_t deviceid)
{
        char * name = NULL;
        xcb_input_xi_query_device_cookie_t xi_cookie;
        xi_cookie = xcb_input_xi_query_device(conn, XCB_INPUT_DEVICE_ALL);

        xcb_input_xi_query_device_reply_t *reply;
        reply = xcb_input_xi_query_device_reply(conn, xi_cookie, NULL);

        xcb_input_xi_device_info_iterator_t it;
        it = xcb_input_xi_query_device_infos_iterator(reply);
        for (; it.rem; xcb_input_xi_device_info_next(&it)) {
            xcb_input_xi_device_info_t *info = it.data;

            if (info->deviceid != deviceid) {
                continue;
            }

            if (info->name_len) {
                name = calloc(info->name_len + 1, 1);
                if (name == NULL) {
                    fprintf(stderr, "Failed to allocate memory.\n");
                    exit(EXIT_FAILURE);
                }

                strncpy(name, xcb_input_xi_device_info_name(info), info->name_len);
                name[info->name_len] = '\0';
                break;
            }
        }

        free(reply);

        return name;
}

static void parse_event(xcb_connection_t *conn, xcb_generic_event_t *event)
{
    xcb_input_hierarchy_info_iterator_t it;
    it = xcb_input_hierarchy_infos_iterator((xcb_input_hierarchy_event_t *)event);
    for (; it.rem; xcb_input_hierarchy_info_next(&it))
    {
        xcb_input_hierarchy_info_t *info = it.data;
        int j = 16;
        while (info->flags && j) {
            int ret;
            char *name = get_device_name(conn, info->deviceid);
            j--;
            ret = handle_device(info->deviceid, info->type, info->flags,
                                name);
            free(name);
            if (ret == -1) {
                break;
            }
            info->flags -= ret;
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

static int running = 1;

static void signal_handler(int signal) {
    (void)signal;
    running = 0;
}

int main(int argc, char *argv[])
{
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
                if (!command) {
                    fprintf(stderr, "Cannot use command %s: %s.\n", optarg, strerror(errno));
                }
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
                "[-v] [-n] [-d] [-0] -c command-prefix\n", argv[0]);
                exit(opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
        }
    }

    xcb_connection_t *conn;
    conn = xcb_connect(NULL, NULL);

    if (conn == NULL) {
        fprintf(stderr, "Can't open X display.\n");
        exit(EXIT_FAILURE);
    }

    const char ext[] = "XInputExtension";

    xcb_query_extension_cookie_t qe_cookie;
    qe_cookie = xcb_query_extension(conn, strlen(ext), ext);

    xcb_query_extension_reply_t *rep;
	rep = xcb_query_extension_reply(conn, qe_cookie, NULL);

    if (!rep || !rep->present) {
        printf("X Input extension not available.\n");
        exit(EXIT_FAILURE);
    }
    xi_opcode = rep->major_opcode;
    free(rep);

    xcb_disconnect(conn);

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

    #ifdef HAVE_DAEMON
    if (!foreground) {
        if (daemon(0, 0) != 0) {
            fprintf(stderr, "Cannot daemonize: %s.\n", strerror(errno));
            #ifdef HAVE_PIDFILE
            pidfile_remove(pfh);
            #endif
            exit(EXIT_FAILURE);
        }
    }
    #else
    fprintf(stderr, "Linked without daemon(), running in foreground.\n");
    #endif

    #ifdef HAVE_PIDFILE
    if (pidfile && pfh) {
        pidfile_write(pfh);
    }
    #endif

    int screen_default_no;
    conn = xcb_connect(NULL, &screen_default_no);

    if (bootstrap) {
        xcb_input_xi_query_device_cookie_t xi_cookie;
        xi_cookie = xcb_input_xi_query_device(conn, XCB_INPUT_DEVICE_ALL);

        xcb_input_xi_query_device_reply_t *reply;
        reply = xcb_input_xi_query_device_reply(conn, xi_cookie, NULL);

        xcb_input_xi_device_info_iterator_t it;
        it = xcb_input_xi_query_device_infos_iterator(reply);
        for (; it.rem; xcb_input_xi_device_info_next(&it)) {
            xcb_input_xi_device_info_t *info = it.data;
            char * name;

            if (info->name_len) {
                name = calloc(info->name_len + 1, 1);
                if (name == NULL) {
                    fprintf(stderr, "Failed to allocate memory.\n");
                    exit(EXIT_FAILURE);
                }

                strncpy(name, xcb_input_xi_device_info_name(info), info->name_len);
                name[info->name_len] = '\0';
            }

            switch (info->type) {
            case XCB_INPUT_DEVICE_TYPE_MASTER_POINTER:
            case XCB_INPUT_DEVICE_TYPE_MASTER_KEYBOARD:
                handle_device(info->deviceid, info->type, XIMasterAdded, name);
                break;
            case XCB_INPUT_DEVICE_TYPE_SLAVE_POINTER:
            case XCB_INPUT_DEVICE_TYPE_SLAVE_KEYBOARD:
                handle_device(info->deviceid, info->type, XISlaveAdded, name);
                handle_device(info->deviceid, info->type, XIDeviceEnabled, name);
                break;
            }

            if (name) {
                free(name);
            }
        }

        free(reply);
    }

    xcb_screen_t *screen = screen_of_display(conn, screen_default_no);
    if (!screen) {
        fprintf(stderr, "Failed to get the screen for the X display");
        exit(EXIT_FAILURE);
    }

    struct {
        xcb_input_event_mask_t head;
        xcb_input_xi_event_mask_t mask;
    } mask;
    mask.head.deviceid = XCB_INPUT_DEVICE_ALL;
    mask.head.mask_len = sizeof(mask.mask) / sizeof(uint32_t);
    mask.mask = XCB_INPUT_XI_EVENT_MASK_HIERARCHY;

    xcb_input_xi_select_events(conn, screen->root, 1, &mask.head);

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

    struct sigaction signal_action = {
        .sa_handler = &signal_handler,
    };
    sigaction(SIGTERM, &signal_action, NULL);
    sigaction(SIGQUIT, &signal_action, NULL);
    sigaction(SIGINT, &signal_action, NULL);
    sigaction(SIGHUP, &signal_action, NULL);

    xcb_flush(conn);

    fd_set fds;
    int xfd = xcb_get_file_descriptor(conn);

    while (running) {
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);

        struct timeval tv = {
            .tv_usec = 0,
            .tv_sec = 10
        };

        int r = select(xfd + 1, &fds, NULL, NULL, &tv);
        if (r < 0) {
            if (errno == EINTR) {
                break;
            }

            perror("select");
            break;
        }

        if (FD_ISSET(xfd, &fds)) {
            if (xcb_connection_has_error(conn)) {
                break;
            }

            xcb_generic_event_t *e;
            while (running && (e = xcb_poll_for_event(conn))) {
                xcb_ge_generic_event_t *ge = (xcb_ge_generic_event_t *)e;

                if (ge->response_type == XCB_GE_GENERIC &&
                    ge->extension == xi_opcode &&
                    ge->event_type == XCB_INPUT_HIERARCHY) {

                        parse_event(conn, e);
                }
                free(e);
            }
        }

    }

    xcb_flush(conn);
    xcb_disconnect(conn);

    return EXIT_SUCCESS;
}
