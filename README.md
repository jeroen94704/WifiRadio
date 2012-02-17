WifiRadio
=========

Introduction
------------

This is the software part of my version of the ubiquitous wifi radio projects. There are many similar builds to be found online, 
and they all use a cheap router as an internet streaming radio, usually running MPD (http://mpd.wikia.com/wiki/Music_Player_Daemon_Wiki) 
to play the music. My build borrows heavily from the radio project by Jeff Keyzer, A.K.A. MightyOhm. He documented the process he 
went through in great detail on his blog, at http://mightyohm.com/blog/2008/10/building-a-wifi-radio-part-1-introduction/

In addition, I also owe a great deal to the SparkFun tutorial "Beginning embedded electronics", which can be found at 
http://www.sparkfun.com/tutorials/57

Contents
--------

The repository contains the following:

	* C-code, to be compiled and programmed onto a suitable AVR microcontroller
	* A perl script, which runs on the router
	* Eagle schematics and board files of the electronics

Features
--------

The firmware+perl script system has the following features:

	* Load a predefined playlist containing a set of internet streams
	* Browse and play from an MP3 collection located on a network share
	* Show currently playing information, including progress in the current track, title/artist info and playlist info

Technical details
-----------------
	
### C-Code

The code was built using the WinAVR suite of tools. You can use the included makefile to build your own binary, which includes targets to set the fuses and program the MPU.

### Perl script

This is a firm departure from Jeff's build, which uses bash scripting to do all the router-side processing. Since I need fairly elaborate two-way communication, involving
a lot of string parsing, using Perl with its built-in regex support was a logical move. The code is a bit dirty in the way it handles 2-way requests (e.g. When the AVR 
asks for a list of tracks). These requests involve receiving a message, processing it, and sending a result back to the AVR. The script forks at startup, so the receiving
and sending parts run in different processes.

When a results needs to be returned to the AVR, the receiver process writes a file containing the response. The sender process detects the existance if this file, reads it, 
sends the data over the serial line, and removes the file. 

More investigation is needed to determine whether the fork is actually necessary. An alternative would be to move to C, and use a proper multithreading approach, but I've 
cracked my skull against setting up an OpenWrt toolchain in the past, and have no immediate desire to attempt this again, also since the current implementation works just fine.