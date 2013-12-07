inputplug
=========

inputplug is a very simple daemon monitoring XInput events and running
scripts on hierarchy changes (such as device attach, remove, enable or
disable).

To build the project, it's best to use [mk-configure(7)](http://github.com/cheusov/mk-configure),
a build system based on [bmake(1)](http://www.crufty.net/help/sjg/bmake.html). However, GNU Makefile
is provided for convenience.

* * *

# NAME

inputplug — XInput event monitor

# SYNOPSIS

__inputplug__ \[__\-a__ _address_\] \[__\-f__ _path_\] \[__\-v__\] \[__\-n__\] __\-c__ _command-prefix_

# DESCRIPTION

__inputplug__ is a daemon which connects to a running X server
and monitors its XInput hierarchy change events. Such events arrive
when a device being attached or removed, enabled or disabled etc.

When a hierarchy change happens, __inputplug__ parses the event notification
structure, and calls the command specified by _command-prefix_. The command
receives three arguments:

* _command-prefix_ _event-type_ _device-id_ _device-type_

Event type may be one of the following:

* _XIMasterAdded_
* _XIMasterRemoved_
* _XISlaveAdded_
* _XISlaveRemoved_
* _XISlaveAttached_
* _XISlaveDetached_
* _XIDeviceEnabled_
* _XIDeviceDisabled_

Device type may be any of those:

* _XIMasterPointer_
* _XIMasterKeyboard_
* _XISlavePointer_
* _XISlaveKeyboard_
* _XIFloatingSlave_

Device identifier is an integer.

Also, if compiled with __libixp__, inputplug can post events to the __wmii__ event file.
To enable __wmii__ support, the address of its __9P__ server needs to be specified.

# OPTIONS

A summary of options is included below.

* __\-v__

    Be a bit more verbose.

* __\-n__

    Start up, monitor events, but don't actually run anything.
    With verbose more enabled, would print the actual command it'd
    run. Currently useless, as __inputplug__ detaches from the terminal
    immediately after start.

* __\-c__ _command-prefix_

    Command prefix to run. Unfortunately, currently this is passed to
    [execvp(3)](http://manpages.debian.org/cgi-bin/man.cgi?query=execvp) directly, so spaces aren't allowed. This is subject to
    change in future.

* __\-a__ _address_

    The address at which to connect to __wmii__. The address takes the
    form _<protocol>_!_<address>_. If an empty string is passed,
    __inputplug__ tries to find __wmii__ automatically.

* __\-f__ _path_

    Path to the event file within __9P__ filesystem served by __wmii__.
    The default is __/event__.

# ENVIRONMENT

* _DISPLAY_

    X11 display to connect to.

* _WMII\_ADDRESS_

    __wmii__ address.

# BUGS

Probably, there are some.

# SEE ALSO

[xinput(1)](http://manpages.debian.org/cgi-bin/man.cgi?query=xinput)

# COPYRIGHT

Copyright (C) 2013, Andrew Shadura.

Licensed as MIT/X11.

# AUTHOR

Andrew Shadura <andrewsh@debian.org>
