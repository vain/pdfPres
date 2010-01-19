pdfPres -- a dual head PDF presenter
====================================

pdfPres is a presentation program for PDF files. It uses a "dual
head"-layout: One window shows the previous, current and next slides
while another window only shows the current slide. That additional
window can be moved to an external screen (such as a beamer) -- use your
window manager to set this window to fullscreen. Thus, you can present
your slides on a beamer while keeping an eye on what's coming up next.

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


Launching
---------

Issue something like:

    $ ./pdfPres -f path/to/slides.pdf [-s <slides>] [-w]

The optional parameter "-s" allows you to specify how many slides
before/after the current slide you wish to see, i.e. "3" means
"preview the next 3 slides while still showing the previous 3 slides".
The default is "2".

The optional parameter "-w" enables wrapping. When you're on the last
slide and wrapping is enabled, switching to the "next" slide actually
switches to the very first slide.

Note: It is no longer needed to specify file pathes with URI's like
"file:///home/user/...". You can use regular pathes like in any other
application.


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
