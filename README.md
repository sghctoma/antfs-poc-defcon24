Help, I've got ANTs!!!
======================

Slides: http://www.slideshare.net/sghctoma/help-ive-got-ants
Videos: https://www.youtube.com/playlist?list=PLO0QV3AGuqc9ikxcKnfnUsVG6IUHSpNJ3

hackant
-------

A tool to demo various ANT features (the three authentication modes, factory
key generation, file upload/download, firmware udpate). This script requires
openant (https://github.com/Tigge/openant) patched with
patch/ant_fs_manager.py.diff!


cfp-application
---------------

My application and a small update sent to the DEF CON 24 CFP team.

firmware
--------

 - firmware.py: A tool to extract the firmware binary the devices accept from
   the RGN file format Garmin distributes them. I'll use this tool for demo #5.

 - fixcrc.py: A tool to fix the checksum of modified Garmin firmwares. This will
   be merged into firmware.py in the near future.

patch
-----

 - ant_fs_manager.py.diff: Patch for OpenANT that implements the "passthrough"
   authentication mode.

 - antfs_client_channel.cpp.diff: The patch for ANT_WrappedLib.dll that makes
   logging passkeys sent by ANT-FS hosts possible.

sniff
-----

 - shockburst.c, packets.h: a simple C program that gets raw 16 bit signed IQ
   data (e.g. from the output of rtl_fm) from standard input, and detects
   ShockBurst packets. These packets are than written to standard out as hex
   pairs. The program should compile with any known compiler (Clang 3.8.0 and
   GCC 4.8.5 tested on HardenedBSD 11-CURRENT, and Clang 3.7.0 tested on Void
   Linux):

   $ clang shockburst.c -o shockburst

 - anteater.py: a Python program that works on shockburst's output, and parses
   ANT-FS packets, which are written to standard output as Python dictionaries
   (this will eventually change to JSON objects)

 - antfs1-annotated.txt, antfs2-annotated.txt: decoded ANT-FS pairing session
   with some comments.

sniff/pothos
------------

PothosWare blocks to decode ShockBurst (ShockBurstDecoder) and ANT-FS
(ANTFSDecoder) packets, and the topology I've used in my demo (ant-sdr.pth)
