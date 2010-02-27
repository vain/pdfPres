pdfPres -- a dual head PDF presenter
====================================

pdfPres is a presentation program for PDF files. It uses a "dual
head"-layout: One window shows the previous, current and next slides
while another window only shows the current slide. That additional
window can be moved to an external screen (such as a beamer) -- use your
window manager to set this window to fullscreen (or press [f]). Thus, you can present
your slides on a beamer while keeping an eye on what's coming up next.

Keeping an eye on the time during your presentations is now possible with a timer.

The visibility of the cursor in the presentation window is disabled by default. 
In case there is no pointing device available the cursor can be set to visible.

To directly jump to a specific slide just type the slidenumber and press [G]
or [RETURN].

Private notes can be viewed in a notepad on the left side. A file containing 
the notes can be loaded with the appropriate open-icon below the notepad.
You can edit your notes from inside pdfPres by pressing the edit
button or [i]. To return to normal mode either press the edit button again
or press [ESC]. Of course, notes can be saved to a file using the save or 
saveAs buttons.

Optionally, an external program can be attached via a pipe. The number
of the slide which is currently shown will be written to that pipe. So,
your external program or script can do additional things depending on
the current slide -- it can show your private notes, trigger sound
events or whatever.

pdfPres uses GTK+ v2 and poppler-glib to render the PDF file. Notes are
handled via libxml2.


Keys
----

* Use the cursor keys to navigate ([Space], [Return] also go the next
  slide).
* [p] switches to "fit page", this is the default.
* [w] switches to "fit to width" mode, [h] switches to "fit to height"
  mode.
* [F5] does a dumb refresh.
* [Escape], [q] quit the program.
* [left mouse] switches to the next slide, [right mouse] switches to the
  previous slide.
* Sometimes you need to browse your slides but that would, inevitably,
  confuse the audience. So fixating the current slide on the beamer
  while still allowing free navigation in the preview window should be
  quite handy. Lock it by pressing [l] and unlock it with a capital [L].
* In locked mode, press [J] to jump to the currently selected slide.
* To switch to fullscreen, press [f]. In gnome [F11] does not work.
* [s] starts the timer, pauses it and continues if paused. [r] resets
  the timer
* [c] toggles cursor visibility in presentation window.
* [i] enters note edit mode.
* [ESC] leaves note edit mode.
* Type a number and then [G] or [RETURN] to jump to the appropriate slide.


Launching
---------

Issue something like:

    $ ./pdfPres [-s <slides>] [-c <cache>] [-w] [-n] path/to/slides.pdf

The optional parameter "-s" allows you to specify how many slides
before/after the current slide you wish to see, i.e. "3" means
"preview the next 3 slides while still showing the previous 3 slides".
The default is "1". 
Attention: It is recommended to set the number of slides not greater
than 2 except if you have a large screen with a high resolution.

The optional parameter "-w" enables wrapping. When you're on the last
slide and wrapping is enabled, switching to the "next" slide actually
switches to the very first slide.

The path has to be the last argument.

"-c" allows you to specify the number of slides that can be
cached/pre-rendered. Be aware, though, that this value will always be at
least "number of pdf viewports" * 2. This is needed so that we can
pre-render the current and next slide for each viewport. Hence,
switching to the next (or previous) slides will always be instant.

Note: It is no longer needed to specify file pathes with URI's like
"file:///home/user/...". You can use regular pathes like in any other
application.

By providing the parameter "-n" the slide which is currently shown will
be written to stdout. You can pipe this information to another program.
That'll allow you to do fancy things.


Configuration
-------------

pdfPres writes all of its configuration variables to
`~/.config/pdfPres/config.xml` on exit. This file will look like this:

	<?xml version="1.0" encoding="UTF-8"?>
	<config>
		<initial_fit_mode v="2"/>
		<slide_context v="1"/>
		<do_wrapping v="0"/>
		<do_notectrl v="0"/>
		<cache_max v="32"/>
		<font_notes v="Sans 12"/>
		<font_timer v="Sans 35"/>
	</config>

For now, you're supposed to directly edit this file except for
`font_notes`. If you change this font inside pdfPres, it'll get saved
automatically. I plan to add a button soon so that you can also change
the timer font.

* `initial_fit_mode` accepts `0`, `1` or `2` meaning `FIT_WIDTH`,
  `FIT_HEIGHT` or `FIT_PAGE`.
* `slide_context` is equivalent to the `-s ...` parameter.
* `do_wrapping` is equivalent to the `-w` parameter. `1` enables
  wrapping and everything else disables it.
* `do_notectrl` is equivalent to the `-n` parameter. `1` enables
  note control and everything else disables it.
* `cache_max` is equivalent to the `-c ...` parameter.


Build instructions
------------------

To build the binary you need [SConstruct](http://www.scons.org/) which
should be included in most linux distributions. Once that's installed,
just grab the source and type:

    $ cd /path/to/sources
    $ scons

That's it. Furthermore, the following external libraries are required:

* [gtk2 >= 2.16.1](http://www.gtk.org)
* [poppler-glib >= 0.10.6](http://poppler.freedesktop.org)
* [libxml2 >= 2.7.6](http://www.xmlsoft.org/)


What do I do with old (non-XML) notes?
--------------------------------------

If you already used an old version of pdfPres that didn't save the notes
in XML, you can use the converter script to transform those notes into
XML:

    $ ./legacy-notes-converter.py notes.txt > notes.xml

The resulting file "notes.xml" can be read in pdfPres.

Be aware that this script expects a file encoded with UTF-8. Use
[Geany](http://www.geany.org/) or
[recode](http://www.gnu.org/software/recode/recode.html) to transform
any non-UTF-8 files (you may adjust the input encoding) before you run
the converter:

    $ recode LATIN1..UTF8 < notes.txt > notes-utf8.txt
