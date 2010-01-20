#!/usr/bin/env python
# -*- coding: utf8 -*-

"""
Copyright 2010 Peter Hofmann

This file is part of pdfPres.

pdfPres is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your
option) any later version.

pdfPres is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with pdfPres. If not, see <http://www.gnu.org/licenses/>.
"""

import sys
import curses

if len(sys.argv) < 2:
	print >> sys.stderr, "Please provide the path to your notes."
	sys.exit(1)

# Read the file.
fp = open(sys.argv[1], "r")
content = fp.read()
fp.close()

# Extract notes.
slides = {}
rawtokens = content.split("-- ")[1:]
for i in rawtokens:
	lines = i.split("\n")
	number = lines[0]
	slides[number] = "\n".join(lines[1:]).strip()

# Setup terminal.
curses.setupterm()

# Loop as long as there's something to read -- and print the requested
# notes.
while True:
	number = sys.stdin.readline()
	if not number:
		sys.exit(0)

	# Clear screen.
	sys.stdout.write(curses.tigetstr("clear"))

	# Announce current slide.
	print ""
	print "\t" + number
	print ""
	print ""

	# Print notes -- if any.
	number = number.strip()
	if number in slides:
		print slides[number]
	else:
		print "--"

	# Flush!
	sys.stdout.flush()
