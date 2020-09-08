inputplug
=========

inputplug is a very simple daemon which monitors XInput events and runs
arbitrary scripts on hierarchy change events (such as a device being
attached, removed, enabled or disabled).

To build the project, run `cargo build`.

* * *

# NAME

inputplug — XInput event monitor

# SYNOPSIS

**inputplug** \[**-v**\] \[**-n**\] \[**-d**\] \[**-0**\] **-c** _command-prefix_

**inputplug** \[**-h**|**--help**\]

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

# OPTIONS

A summary of options is included below.

* **-h**, **--help**

    Show help (**--help** shows more details).

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

* **-p** _pidfile_

    Write the process ID of the running daemon to the file _pidfile_

# ENVIRONMENT

* _DISPLAY_

    X11 display to connect to.

# BUGS

The Rust port doesn't support WMII.

# SEE ALSO

[xinput(1)](http://manpages.debian.org/cgi-bin/man.cgi?query=xinput)

# COPYRIGHT

Copyright (C) 2013, 2014, 2018, 2020 Andrej Shadura.

Copyright (C) 2014, 2020 Vincent Bernat.

Licensed as MIT/X11.

# AUTHOR

Andrej Shadura <andrewsh@debian.org>
