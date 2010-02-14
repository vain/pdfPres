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

Private notes can be viewed in a notepad on the left side. A file containing 
the notes can be loaded with the appropriate open-icon below the notepad.
The format of the file is descibed later in this document.

Optionally, an external program can be attached via a pipe. The number
of the slide which is currently shown will be written to that pipe. So,
your external program or script can do additional things depending on
the current slide -- it can show your private notes, trigger sound
events or whatever.

pdfPres uses GTK+ v2 and poppler-glib to render the PDF file.


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
That'll allow you to do fancy things. "private-notes.py" is an example
of what you could do ("example" meaning "quick and dirty"):

    $ ./pdfPres [-s <slides>] [-w] -n path/to/slides.pdf | ./private-notes.py ~/presentation/my-notes.txt

This will attach the script to pdfPres. 

Currently, the script and the private notes function expect a
file formatted like this:

	-- 1
	This text will be shown on the first slide.

	-- 2
	... notes for second slide ...

	-- 23
	Here be dragons. Shown on slide 23.

To be more precise, a line containing "-- [n]" indicates that the
following lines shall be displayed on the n'th slide.


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
