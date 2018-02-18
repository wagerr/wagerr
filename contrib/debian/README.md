
Debian
====================
This directory contains files used to package wagerrd/wagerr-qt
for Debian-based Linux systems. If you compile wagerrd/wagerr-qt yourself, there are some useful files here.

## wagerr: URI support ##


wagerr-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install wagerr-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your wagerrqt binary to `/usr/bin`
and the `../../share/pixmaps/wagerr128.png` to `/usr/share/pixmaps`

wagerr-qt.protocol (KDE)

