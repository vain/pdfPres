# -*- coding: utf-8 -*-
import os

# Set up environment, pkg-config for GTK/poppler, ...
env = Environment(
	CCFLAGS =  os.environ.get('CFLAGS', '') + ' -Wall -Wextra',
	LINKFLAGS = os.environ.get('LDFLAGS', ''))
env.ParseConfig('pkg-config --cflags --libs gtk+-2.0 poppler-glib cairo')
env.ParseConfig('xml2-config --cflags --libs')

# Version number of pdfpres:
env.Append(CPPDEFINES = {'PDFPRES_VERSION' : '\\"' +
	os.popen('git describe').readline().strip() + '\\"'})

# Build instructions
env.Program('pdfpres', Glob('*.c'))
