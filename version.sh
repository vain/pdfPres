#!/bin/sh

if which git >/dev/null 2>&1
then
	if git status >/dev/null 2>&1
	then
		echo '-DPDFPRES_VERSION="'"$(git describe)"'"'
		exit 0
	fi
fi

exit 1
