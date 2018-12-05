# albumattr
version 2.0.0 (3.12.2004)

##### introduction.
Sets a new set of attributes for music albums. It will set the MIME type of a directory containing an album to "application/x-vnd.Be-directory-album". It will also set the new attributes Album:Artist, Album:Title, Album:Length, and Album:Year according to the contents of the Audio:* attributes of the songs in that album. Furthermore, it can also find the cover image in the album directory and copy a thumbnail of it into the album directory's icon.
It only works on albums - if it finds different artists/album titles, it will skip the directory - but you can force it to work in this case (useful for samplers).
It can be used as a Tracker add-on and as a command line utility.

##### requirements.
Haiku or BeOS R5 is required for this application. If you want to use it on a PowerPC you need the Metrowerks C++ Compiler to create the executable (you may have to change the makefile to use this compiler, sorry).

##### installation.
You can copy the application "albumattr" wherever you want to. If you want to use it frequently, you should place it within your path, e.g. /boot/home/config/bin.
The Tracker add-on "Album Folder Attributes" should be in /boot/home/config/add-ons/Tracker/. Since there is only one file, you can either copy it to both locations, or create a symlink from one to the other.
The "Install" script part of this archive will it install it in the former way, so that you can safely rename the Tracker add-on to suit your needs.

##### usage.
If you run "albumattr" without any arguments, a short help message is printed.
```sh
albumattr [-vrmfictds] <list of directories>
	-v	verbose mode
	-r	enter directories recursively
	-m	don't use the media kit: retrieve song length from attributes only
	-f	forces updates even if the directories already have attributes
	-i	installs the extra application/x-vnd.Be-directory-album MIME type
	-c	finds a cover image and set their thumbnail as directory icon
	-t	don't use the thumbnail from the image, always create a new one
	-d	allows different artists in one album (i.e. for samplers, soundtracks, ...)
	-s	read options from standard settings file
```
If you use it as a Tracker add-on, it will check if the Album Folder MIME type is installed, and will install it first, it not. Unlike the command line version, the Tracker add-on has the -c option turned on by default.
You can now also get to a settings window when you press the Control key while selecting the add-on in Tracker. All changes you made there are permanent, and they can also be used by the command line tool when the -s option is used.
When you press the Shift key when you select the add-on in Tracker, it will turn on the -f flag, that is, it will update the attributes/icon even if they already exist.

##### history.
version 1.0.0 (15.6.2003)
	- initial release.

version 1.1.0 (25.6.2003)
 - capability to be used as Tracker add-on added.

version 1.2.0 (23.10.2003)
 - it no longer creates an application when used as Tracker add-on.

version 1.3.0 (12.12.2003)
 - now also sets the Album:Genre attribute.
 - if used as a Tracker add-on, it will now ask if it should proceed if there are different artists or albums set.

version 2.0.0 (3.12.2004)
 - now has a settings window.
 - the Tracker add-on can now also use the "force" option by pressing shift
 - can copy cover image thumbnails into the album icons.

##### author.
"albumattr" is written by Axel Dörfler <axeld@pinc-software.de>.
visit: www.pinc-software.de

Have fun.
