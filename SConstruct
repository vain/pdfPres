# -*- coding: utf-8 -*-
import os
import subprocess

# Set up environment, pkg-config for GTK/poppler, ...
env = Environment(
	CCFLAGS =  os.environ.get('CFLAGS', '') + ' -Wall -Wextra',
	LINKFLAGS = os.environ.get('LDFLAGS', ''))
env.ParseConfig('pkg-config --cflags --libs gtk+-2.0 poppler-glib cairo')
env.ParseConfig('xml2-config --cflags --libs')

# Version number of pdfpres:
process = subprocess.Popen('./version.sh', shell=False, stdout=subprocess.PIPE)
ver = process.stdout.readline().strip()
process.stdout.close()
if process.wait() == 0:
	env.Append(CPPDEFINES = {'PDFPRES_VERSION': ver})

# Build instructions
env.Program('pdfpres', Glob('*.c'))
