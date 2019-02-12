inputplug
=========

inputplug is a very simple daemon which monitors XInput events and runs
arbitrary scripts on hierarchy change events (such as a device being
attached, removed, enabled or disabled).

To build the project, it's best to use [mk-configure(7)](http://github.com/cheusov/mk-configure),
a build system based on [bmake(1)](http://www.crufty.net/help/sjg/bmake.html). If you can’t use
it for some reason, a GNU Makefile is also included.

* * *

# NAME

inputplug — XInput event monitor

# SYNOPSIS

**inputplug** \[**-a** _address_\] \[**-f** _path_\] \[**-v**\] \[**-n**\] \[**-d**\] \[**-0**\] **-c** _command-prefix_

# DESCRIPTION

**inputplug** is a daemon which connects to a running X server
and monitors its XInput hierarchy change events. Such events arrive
when a device is being attached or removed, enabled or disabled etc.

When a hierarchy change happens, **inputplug** parses the event notification
structure, and calls the command specified by _command-prefix_. The command
receives four arguments:

* _command-prefix_ _event-type_ _device-id_ _device-type_ _device-name_

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

Device identifier is an integer. The device name may have embedded spaces.

Optionally, if compiled with **libixp**, inputplug can post events to the **wmii** event file.
To enable **wmii** support, the address of its **9P** server needs to be specified.

# OPTIONS

A summary of options is included below.

* **-v**

    Be a bit more verbose.

* **-n**

    Start up, monitor events, but don't actually run anything.
    With verbose more enabled, would print the actual command it'd
    run. This implies **-d**.

* **-d**

    Don't daemonise. Run in the foreground.

* **-0**

    On start, trigger added and enabled events for each plugged devices. A
    master device will trigger the "added" event while a slave device will
    trigger both the "added" and the "enabled" device.

* **-c** _command-prefix_

    Command prefix to run. Unfortunately, currently this is passed to
    [execvp(3)](http://manpages.debian.org/cgi-bin/man.cgi?query=execvp) directly, so spaces aren't allowed. This is subject to
    change in future.

* **-a** _address_

    The address at which to connect to **wmii**. The address takes the
    form _<protocol>_!_<address>_. If an empty string is passed,
    **inputplug** tries to find **wmii** automatically.

* **-f** _path_

    Path to the event file within **9P** filesystem served by **wmii**.
    The default is **/event**.

# ENVIRONMENT

* _DISPLAY_

    X11 display to connect to.

* _WMII\_ADDRESS_

    **wmii** address.

# BUGS

Probably, there are some.

# SEE ALSO

[xinput(1)](http://manpages.debian.org/cgi-bin/man.cgi?query=xinput)

# COPYRIGHT

Copyright (C) 2013, 2014, 2018 Andrej Shadura.

Copyright (C) 2014 Vincent Bernat.

Licensed as MIT/X11.

# AUTHOR

Andrej Shadura <andrewsh@debian.org>
