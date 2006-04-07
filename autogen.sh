#!/bin/sh

aclocal -I m4 && autoheader && autoconf && automake --gnu --add-missing --copy
