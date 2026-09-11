<!--
.\" Copyright (C) 2026 VirtualSquare. Project Leader: Renzo Davoli
.\"
.\" This is free documentation; you can redistribute it and/or
.\" modify it under the terms of the GNU General Public License,
.\" as published by the Free Software Foundation, either version 2
.\" of the License, or (at your option) any later version.
.\"
.\" The GNU General Public License's references to "object code"
.\" and "executables" are to be interpreted as the output of any
.\" document formatting or typesetting system, including
.\" intermediate and printed output.
.\"
.\" This manual is distributed in the hope that it will be useful,
.\" but WITHOUT ANY WARRANTY; without even the implied warranty of
.\" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
.\" GNU General Public License for more details.
.\"
.\" You should have received a copy of the GNU General Public
.\" License along with this manual; if not, write to the Free
.\" Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
.\" MA 02110-1301 USA.
.\"
-->
# NAME

`libvdeplug_restart` — VDE nested module with automatic restart

# SYNOPSIS

`libvdeplug_restart.so`

# DESCRIPTION

This nested module automatically restarts a VDE module in the event of a service failure.

This module is part of libvdeplug4 and can be used by any program supporting
VDE, such as `vde_plug`, `vdens`, `kvm`, `qemu`, `user-mode-linux`, and
`virtualbox`.

The `vde_plug_url` syntax for this module is:

`restart://`[*polling_period_in_seconds*]`{`*vde_nested_url*`}`

The default polling period is 20 seconds.

# OPTIONS

This module has no options.

# EXAMPLES

```
restart://{vde:///tmp/sw}
```

This configuration checks every 20 seconds whether the connection to the switch
is operational. If a failure is detected, the module attempts to reconnect to
the switch every 20 seconds.

```
restart://3{vde:///tmp/sw}
```

This is equivalent to the previous example, except that the polling period for
checking the service and attempting reconnection is 3 seconds instead of the
default 20 seconds.

# NOTICE

Virtual  Distributed  Ethernet  is not related in any way with www.vde.com ("Verband der Elektrotechnik, Elektronik
und Informationstechnik" i.e. the German "Association for Electrical, Electronic & Information Technologies").

# SEE ALSO

`vde_plug`(1)

