# vdeplug\_restart
VDE connection self-restarting nested module

This  nested  module automatically restarts a VDE module in the event of a
     service failure.

This module of libvdeplug4 can be used in any program supporting VDE like vde\_plug, vdens, kvm, qemu, user-mode-linux and virtualbox.

## install vdeplug\_restart

Requirements: [vdeplug4](https://github.com/rd235/vdeplug4).

vdeplug\_restart uses cmake, so the standard procedure to build and install
this vdeplug plugin module is the following:

```sh
$ mkdir build
$ cd build
$ cmake ..
$ make
$ sudo make install
```

## usage examples (tutorial)

The following examples are VNLs (Virtual Network Locator) to be used with programs
supporting vde as specified by the syntax of those programs.

### restart the `cable` to the switch
```restart://{vde:///tmp/sw}```

This configuration checks every 20 seconds whether the connection  to  the
switch  is  operational.  If a failure is detected, the module attempts to
reconnect to the switch every 20 seconds

### restart the `cable` to the switch (3 seconds polling)
```restart://3{vde:///tmp/sw}```

This is equivalent to the previous example, except that the polling period
for checking the service and attempting reconnection is 3 seconds  instead
of the default 20 seconds.


See the man page (libvdeplug\_restart) for more information.
