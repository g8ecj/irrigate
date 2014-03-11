This is the irrigation controller. It has evolved over a period of some 5 years, the
requirements have changed (and been expanded) and the hardware changed such that a router
no longer requires the warranty invalidated by modifying it with a soldering iron!!

It uses a number of DS2413 1-wire dual channel GPIO chips to drive triacs to operate
valves and pumps. It also optionally monitors 2 temperature sensors to implement 
a preset frost protection program and monitors the current drawn by the valves to 
ensure they are connected correctly.

The 1-wire bus with these devices on it is driven from a DS2480B UART to 1-wire Master 
Controller chip which is connected to one of the serial ports available on a modified
Linksys WRT54GL wireless router. It has also been used with a TPLINK WR1043ND router
and a USB9097 USB 1-wire master dongle.

The router has Open Source software available for it that not only frees the 
user from the limits of the manufacturers code but also provides the cross compile 
environment to allow addition functions to be added by anyone.
See http://openwrt.org

It uses the owfs system that makes 1-wire devices appear as a part of the file system.
The interface to the owlib software is via the owcapi 'C' API library.

These files implement an embedded http server with simple callbacks to the main code. It
is licensed under the GPL V2 License. I'm using version 5.3 at present.
```
   mongoose.c
   mongoose.h
```

These files are totally new and provide all of the AJAX web interface, the onewire interface,
statistics, time event queueing, scheduling, command line parsing and error handling.
They are released under the GNU Public License version 2.
```
   irr_actions.c
   irr_chanmap.c
   irr_onewire.c
   irr_parse.c
   irr_queue.c
   irr_schedule.c
   irr_stats.c
   irr_utils.c
   irr_web.c
   irrigate.c
```
External libraries are used to parse and generate JavaScript Object Notation (JSON) data for the AJAX 
methods used in the web interface. The javascript widgets for timeline support are from 
the CHAPS links library and are available under the Apache license. Other javascript code has
been released to the public domain.

The makefile will do a native build under Linux by defining a 'PC' target that causes a 1-wire simulator
to be built, allowing the web interface to be tested.

Compiling under OpenWrt is simple. Unpack the compressed file 'openwrt-package' that provides the 
package makefile and control files for native packaging and installation into the 'package'
directory. Configure the system to build the package by running 'make menuconfig' and select
```
    'Utils' -> 'irrigate'
            -> 'Filesystem' -> 'owfs'
    'Libraries' -> 'libjson'
                -> 'Filesystems' -> 'libow'
                                 -> 'libow-capi'
```
These can all be built as modules or built into the main image.

Kernel modules to consider depend on the 1-wire interface in use. The USB9097 requires the 
'kmod-usb-serial-ch341' module USB/UART driver for example.

Save the new config and then build the whole system using 'make'. Install to the target as a
complete image or using 'opkg' package manager if using modules.

Files to be created/customised are:
```
www/passfile
www/zones.html
www/zones.jpg
www/zones.conf
```

The first is created using mongoose in stand-alone mode, the next 2 describe the layout of the general 
webpage and the final file describes the 1-wire addesses (where appropriate), the characteristics of 
each zone and where it appears on the browser image map.

```
$ ./irrigate --help
irrigate: Irrigation Controller Version 3.10 Copyright (c) 2009-2014 Robin Gilks
usage: ./irrigate [bcdfhlprstvx]

-a <file>, --file      access log file name
-b,        --background run in daemon mode
-c,        --config    configure zone mapping
-d <dir>,  --datapath  data file directory
-f <file>, --file      config file name
-l <int>,  --limit     degree-minutes before frost program runs
-m,        --monitor   check current drawn by solenoid valves
-p <int>,  --port      http port number
-r <dir>,  --root      http root directory
-s <dev>,  --serialport serial port device
-t <int>,  --threshold temperature below which frost protect operates
-v,        --version   useful version information
-x <int>   --debug     debug mode
-h,        --help      this help screen
```
Example command line:
```
irrigate -m -b -t 0.1 -l 15 -r /www -d /www -f /www/zones.conf -s /dev/tts/1
   * monitor valve current usage
   * run in background as a service
   * threshold temperature for frost protection -0.1C
   * run frost protect after 15 degree-minutes (eg -3C for 5 minutes)
   * root directory /www
   * data directory (hold history, schedule and statistics data)
   * configuration file path and name defined
   * serial port used by modified WRT54GL (second serial port)
```

TODO

allow use of either well pump on its own or domestic feed on its own

