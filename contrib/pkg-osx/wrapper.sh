#!/bin/sh

## jack is optional,... for now.
## this wrapper is still required to cd to the bindir for the font-file

#if test ! -x /usr/local/bin/jackd -a ! -x /usr/bin/jackd ; then
#  /usr/bin/osascript -e '
#    tell application "Finder"
#    display dialog "You do not have JACK installed. xjadeo will not run without it. See http://jackaudio.org/ for info." buttons["OK"]
#    end tell'
#  exit 1
#fi

progname="$0"
curdir=`dirname "$progname"`
progbase=`basename "$progname"`
execname="${progbase}-bin"

if test -x "${curdir}/$execname"; then
 cd "${curdir}"
 exec "./${execname}" "$@"
fi
