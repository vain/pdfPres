#!/usr/bin/env python
# -*- coding: utf8 -*-

"""
Copyright 2010 Peter Hofmann

This file is part of pdfpres.

pdfpres is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your
option) any later version.

pdfpres is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with pdfpres. If not, see <http://www.gnu.org/licenses/>.
"""

import sys
from xml.dom import minidom

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
	# Filter out commented lines.
	lines = [e for e in lines if (e != "" and e[0] != "#") or e == ""]
	# Set notes for this slide.
	slides[number] = "\n".join(lines[1:]).strip()

# Create and print XML document.
xml = minidom.Document()

rootElem = xml.createElement("notes")
xml.appendChild(rootElem)

for (slideNum, note) in slides.items():
	# We expect the file to be UTF-8-encoded.
	note = note.decode("UTF-8")

	slideElem = xml.createElement("slide")
	slideElem.setAttribute("number", slideNum)

	textData = xml.createTextNode(note)

	slideElem.appendChild(textData)
	rootElem.appendChild(slideElem)

print xml.toxml("UTF-8")
