#!/bin/bash
#
# (C) 2005 robin@gareus.org
#
# distribute in terms of GPL
#
# simple script that loops through available mencoder
# codecs and checks if xjadeo supports playback
#
# TODO:
# * also loop through transcode export codecs.
# * allow ./encoder.sh  [<video-file>]  [mencoder options]
# * unhardcode example file path.
# * default to mpeg4 instead of mjpeg ?
#

function check_execs {
  if [ -z "$XJADEO" ]; then
   if [ -x ../src/xjadeo/xjadeo ]; then
    XJADEO=../src/xjadeo/xjadeo
   else 
    XJADEO=$(which xjadeo)
   fi
  fi
 
  MENCODER=$(which mencoder)

  if [ -z "$MENCODER" -o -z "$XJADEO" ]; then
   echo "xjadeo or mencoder executable not found"
  fi
  echo "using $XJADEO"

  if [ -f ./xjadeo-example.avi ]; then
    EXAMPLE=./xjadeo-example.avi
  else
    EXAMPLE=/usr/share/doc/xjadeo/contrib/xjadeo-example.avi
  fi

}

function usage {
 echo "$0 [<video-file>]"
 echo " if no file is specified, a sample video provided with the source is used."
}

function cleanup {
 echo
 rm $OUTPUTFILE
 exit 
}

function supported {
  echo -n "supported"
  if [ -z "$USE" ]; then 
    USE=$codec; 
  fi; 
}



if [ "$1" = "-h" ]; then 
 usage; exit;
fi

check_execs;

INPUTFILE=$1
if [ -z "$1" ]; then 
 INPUTFILE=$EXAMPLE;
fi

if [ ! -f $INPUTFILE ]; then 
 echo "file not found: '$INPUTFILE'"; exit;
fi


OUTPUTFILE=`mktemp -t jadeo.XXXXXX` || exit 
USE=""

trap cleanup 15 9 1 2

OPTS_GEN="-nosound -endpos 1"
OPTS_SCALE="-vf scale=160:120"
OPTS_CODEC="-ovc lavc -lavcopts keyint=1"
#EXTRA_OPTS="-idx vbitrate=800 gray"

VCODECS="mjpeg ljpeg dvvideo flv ffv1 mpeg4 h261 h263 h263p msmpeg4 msmpeg4v2 wmv1 wmv2 rv10 mpeg1video mpeg2video huffyuv ffvhuff asv1 asv2 svq1 snow"

for codec in $VCODECS; do 

 echo -n "trying codec: $codec - "

 $MENCODER $OPTS_GEN $OPTS_CODEC \
   -lavcopts vcodec=$codec $OPTS_SCALE \
   $INPUTFILE -o $OUTPUTFILE  &>/dev/null

 $XJADEO -q -t $OUTPUTFILE &>/dev/null \
  && supported \
  || echo -n "not supported"

 echo; 
done

if [ -n "$USE" ]; then
 echo
 echo "suggesting the use of $USE."
 echo "encode the File with:"
 echo " mencoder -nosound -vf scale=160:120 -ovc lavc \\"
 echo "          -lavcopts keyint=1 -lavcopts vcodec=$USE \\"
 echo "          $INPUTFILE -o output-file.avi"
 echo
 echo "adjust '-vf scale=width:height' "
 echo "adjust input-file: (here: $INPUTFILE) "
 echo "or remove the option to use size of source file or mencoder default"
 echo "see 'man mencoder' for more advanced options"
else 
 echo "Could not find a suitable codec."
 echo "check xjadeo installation."
 echo "print out 'man mencoder' or use transcode."
fi

cleanup
