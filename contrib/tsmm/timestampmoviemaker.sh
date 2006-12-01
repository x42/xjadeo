#!/bin/sh
if test -n "$1" ; then OUTFILE=$1
else OUTFILE="/tmp/test.avi"
fi

VCODEC=mpeg4
FPS=25
TEMP=`mktemp -d -t xjtsmm.XXXXXXXXXX` || exit 1

#if test -f $OUTFILE ; then exit 1; fi
##trap SIGINT { rm -rf $TEMP ; } 

echo "output file: $OUTFILE - codec: $VCODEC"
./xjtsmm $TEMP || exit 1

echo 
echo "encoding tif -> jpg"
for frame in $(ls $TEMP/*.tif); do
	echo -ne "$frame     \r"
	convert $frame  $frame.jpg
done

echo
echo "encoding jpg -> $OUTFILE"
mencoder "mf://$TEMP/*.jpg" -mf fps=$FPS -o $OUTFILE -ovc lavc -lavcopts vcodec=$VCODEC

echo 
echo "cleaning up $TEMP"
rm -f $TEMP/*.tif
rm -f $TEMP/*.jpg
rmdir $TEMP
