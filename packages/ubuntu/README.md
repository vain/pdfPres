How to build this package
=========================

1. Create a folder named `debpackage`.
2. In `debpackage`, create another folder named `DEBIAN`.
3. Copy the `control` file to `debpackage/DEBIAN/`.
4. In `debpackage`, create the nested folders `usr/bin`.
5. Copy the compiled binary to `debpackage/usr/bin/`.

Now, you should end up with a directory structure like this:

	debpackage/
	|-- DEBIAN
	|   `-- control
	`-- usr
	    `-- bin
	        `-- pdfPres

Issue the following command to build the package:

	$ dpkg -b debpackage pdfpres.deb
