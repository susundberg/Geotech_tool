Geotech_tool
============

Linux userspace program to communicate with usb connected gps watch 'geotech'

For more information, see https://sites.google.com/site/paulisundberg/geotech-gps-tool .



== Usage
Run program without any parameters to see program usage help.


== Compiling

Program is not using any fancy libraries but standard C-libraries. The 
compiling system is using Cmake, and building should be only steps:

```
mkdir build 
cd build
cmake ../
make
```

There is ready made directory 'build' that contains cmake generated makefile 
with x86_64 Ubuntu 11.04.

The binary that is produced is stand-alone in the sense that it can be copied to any system
directory if such is wanted (like /usr/local/bin).

== Sources
* datafile.c -- Contains functions for printing GPX files
* logging.c  -- Contains functions for pretty debug printing
* main.c     -- Main program structure and run mode selection 
* serial.c   -- Actuall communication code with device
* messages.h -- The messages for communication with device

== History log before GitHub
2012-09-30 Sundberg: Fixed issue with time stamp after 16:00 hours from midnight, thanks to Michal Kaut, Norway
2012-09-14 Sundberg: Fixed issues with serial reading, thanks to Erik Starbäck, Sweden
2012-04-14 Sundberg: First version created.

