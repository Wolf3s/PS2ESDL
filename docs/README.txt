Playstation 2 External Storage Device games Loader (PS2ESDL) v0.825 Open-beta readme
=======================================================================================
Last updated: 2014/05/17

Preface:
--------
PS2ESDL was started by me in March 2009, around when Ifcaro had launched his USBLD project.

I had became inspired by him, and also tinkered around with USBAdvance.
However, I soon saw that USBAdvance had no future with the way it has been written.

I found resources, like romz's CDVDMAN/CDVDFSV disassembly, and the source code of HDProject v1.07 and later v1.081.

Although later, I've rewritten PS2ESDL's core (From ground up), so HDProject v1.081 is now more or less not present (PS2ESDL's EE core only shares a very mild resemblence of it).

This PS2ESDL source is compiliable by the *latest* generic PS2SDK (As of today).
No modification needs to be made to your toolchain, except for adding ps2-packer, and linking with LIBPNG, GSKIT and all the necessary dependencies.

There is now a new GUI that accepts a custom background, and PS2ESDL is designed to be launched with any possible launching method that can launch ELFs properly.

PS2ESDL uses the latest USBD and USBHDFSD drivers. However, I've customised them (Gotten rid of any unnecessary functions).

PS2ESDL got terminated in January 2010 for several personal reasons.
However, as I tinkered with the code slowly in my free time, I've managed to get it working on 12th Feburary 2010. The project has since resumed.
As PS2ESDL's name suggests, I'm trying to make it specialise in loading games from the PS2's external interfaces like the USB or i.Link.

********************

A short guide on how to launch/use PS2ESDL
==========================================
-Install your games onto a USB disk (Prefably a hard disk) that was formatted with the FAT32 filesystem.
-Defragment your disk after you install your games.
	PS2ESDL assumes that your game files are contiguous. Therefore FILE FRAGMENTATION IS UNACCEPTABLE!!!!!
-Launch PS2ESDL in your favourite way.
-Plug in your USB disk that has your games on it.
-Start your game by pressing either the CIRCLE, CROSS or START buttons
	Hold the necessry buttons/triggers to invoke the necessary cache size settings and compatibility modes as you do so.

** About the patches.ppi file that comes with PS2ESDL: Put it in the same directory where you're launching PS2ESDL from (It's necessary for compatibility with some games)!
*** Please visit http://ichiba.geocities.jp/ysai187/PS2/PS2ESDL/guide to view the full guide (With colour photographs).

********************
Additional Notes:
=================
-PS2ESDL supports it's own game format, the PS2ESDL version 1.22 format.
-It also supports the traditional USBExtreme format.
-Your USB disk must be FAT32 formatted, defragmented and have NO bad sectors (Your image file(s) may be "fragmented" then!).

Features unique to PS2ESDL:
===========================

The PS2ESDL v1.22 game format:
------------------------------
-Stores games in the "PS2ESDL" (Without the quotes) folder on your USB disk.
-Uses 1.74GB disc image slices (Less files present on your disk for each game).
-Stores certain settings (E.g. cache size, syscall unhook setting etc.) for each individual game.
	No need to memorise, and press a multi-button combination when starting your games!
-Supports Japanese (SHIFT-JIS X208) titles.
-Titles can be up to 40 characters in length.
-CRC32 checksum support for checking for corruption.

User-customizable UI (Background + Skin):
-----------------------------------------
-Background loading:

Place your 640x448 PNG background as either:

	a) mc0:PS2ESDL/background.png
	b) mc1:PS2ESDL/background.png
	c) The place where PS2ESDL was launched from>/background.png
	d) mass:PS2ESDL/background.png

This is also the order in which PS2ESDL will search for a loadable background.
The background can be smaller in size (It won't be stretched nor centered though), but it's width and height MUST be divisible by 4.
>>>PS2ESDL will not load and display images that exceed a size of 640x448.

-Skin loading:

Place your skin (UI.png and UI.dat, or just only UI.png if your skin doesn't have a configuration file) in the same folder where PS2ESDL is launched from.
The skin should not be larger than 640x448 in size.

DMA/unaligned buffer data transfer cache setting:
------------------------------------------------
-You may specify the cache size used by CDVDMAN for DMA/unaligned buffer data transfers (For improved performance).
-Custom cache size setting (Larger caches generally mean better performance, but may break some memory-hungry games):

L1/L2/R1/R2 -> 8/16/24/32 sector cache sizes respectively.

Compatibility modes
-------------------
-TRIANGLE: Unhook syscalls (aka. HDLoader's Mode 3)
-SQUARE: Force synchronization (Needed for a few games that time-out because the USB is too slow).

To invoke a custom cache size setting and/or any required compatibility mode(s), just hold the corresponding trigger/button as you press CIRCLE, CROSS, or START to launch your game.

What you should not see (And won't want to see): The game freezes (Or behaves erratically), or skips some of it's videos. It's because the cache is too large, and too much memory has been consumed.

Please let me know what cache size works best for your games! Results should vary from game to game, but most of my Japanese games (Those that stream their video data to the EE through DMA transfers) had positive performance improvements with a bigger sector cache.
Please start testing with the 8-sector cache, and then go for larger cache sizes.

*** When making a compatibility report, please state whether the game works with all options off (No triggers pressed), and what cache size it works best with (Optional!). ***

Options screen and other options:
=================================
-Press SELECT to bring up the options screen.
-Press R1+SELECT to exit PS2ESDL and return back to OSDSYS.

===========================================================

Hardware tested on:
-------------------
Playstation 2 consoles:
	-SCPH-77006 (Unmodified).
	-SCPH-39006 (M-Chip MP188).
	-SCPH-15000 Playstation 2 console (Unmodified).
-USB HDD(s): Smart-Drive (IEEE1394 + USB 2.0 enclosure). Contains an Oxford 934 chipset.
-Disk defragmentation utility: Auslogics Disk Defragmenter

PS2ESDL was booted with uLE v4.42d and FMCB 1.93 (Installed on an imitation 8MB Memory Card) on these consoles.
*****************************************************

Links/Downloads
===============
PS2ESDL's official homepage: http://ichiba.geocities.jp/ysai187/PS2/PS2ESDL/
PS2ESDL's official game compatibility page: http://ichiba.geocities.jp/ysai187/PS2/PS2ESDL/compatibility.htm
PS2ESDL's official user guide: http://ichiba.geocities.jp/ysai187/PS2/PS2ESDL/guide/

Please feel free to contact me if any of these instructions are vague (Or confusing), or you've got suggestions of any kind!

All the best and have fun!
-SP193, 2014/05/17
