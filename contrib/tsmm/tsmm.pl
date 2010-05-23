#!/usr/bin/perl

$FPS="25";
#$FPS="29.97";
#$FPS="30";
#$FPS="24";
#$FPS="23.976";
#$FPS="24.976";

if ($#ARGV == 0) {
  $FPS=$ARGV[0];
}

$DURATION="10:10:10"; 

#$VCODEC="theora"; # ffmpeg2theora
#$VCODEC="libtheora";
#$VCODEC="mpeg4";
$VCODEC="mjpeg";
#$VCODEC="msmpeg4v2";
#$VCODEC="libx264";

#$OUTFILE="/tmp/tsmm-$FPS.ogv";
$OUTFILE="/tmp/tsmm-$FPS.avi";
#$OUTFILE="/tmp/tsmm-$FPS.mov";

$TEMP=`mktemp -d -t xjtsmm.XXXXXXXXXX` || die;
chomp $TEMP;
#$TEMP="/tmp/xjtsmm.BwZhRX5448";

# TODO trap sigint -> clean up
# sub catch_zap {
# 	die;
# }
# $SIG{HUP} = \&catch_hup;
# $SIG{INT} = \&catch_zap;
# #$SIG{QUIT} = \&catch_zap;
# $SIG{KILL} = \&catch_zap;
# $SIG{TERM} = \&catch_zap;
#

print "output file: $OUTFILE - codec: $VCODEC\n";

if (1) {
	print "1) generating tif images in folder $TEMP\n";
	@args = ("./xjtsmm", "$TEMP", "$FPS", "$DURATION");
	system(@args);
	if ($? != 0) { die "failed to execute: $!\n"; }
}

my @files;

if (0) {
  # required for mencoder
	print "\n2) encoding tif -> jpg\n";
	opendir(DIR,$TEMP);
	@files = grep { /\.tif$/ } readdir (DIR);
	closedir(DIR);
	$i=0;
	if ($#files > 0) {
		printf("frames left to convert: %i\n",$#files);
		foreach (@files) {
			my $frame = $_; 
			printf ("%02.2f%% %s     \r", ($i*100.0/$#files) , $frame);
			system("convert $TEMP/$frame $TEMP/$frame.jpg");
			if ($? != 0) { die "failed to execute: $!\n"; }
			unlink ("$TEMP/$frame");
			$i++;
		}
	}
  $SUFFIX=".tif.jpg";
  $DELSFX=".jpg";
} else {
	print "\n2) skipped converting to jpg.\n";
	opendir(DIR,$TEMP);
	@files = grep { /\.tif$/ } readdir (DIR);
	closedir(DIR);
  $SUFFIX=".tif";
  $DELSFX="";
}

if ("$FPS" eq "29.97")  { $FPS="30000/1001"; print "using 30000/1001 for 29.97\n"; }
if ("$FPS" eq "23.976") { $FPS="24000/1001"; print "using 24000/1001 for 23.976\n"; }
if ("$FPS" eq "24.976") { $FPS="25000/1001"; print "using 25000/1001 for 24.976\n"; }
if ("$FPS" eq "24.975") { $FPS="25000/1001"; print "using 25000/1001 for 24.975\n"; }
if ("$FPS" eq "59.94")  { $FPS="60000/1001"; print "using 60000/1001 for 59.94\n"; }

print "\n3) encoding jpg -> $OUTFILE\n";
my @args;
if    ($VCODEC eq "theora") {
	@args = ("ffmpeg2theora", "$TEMP/frame_%7d$SUFFIX", "--inputfps", "$FPS", "-F", "$FPS", "-o","$OUTFILE");
} elsif ($VCODEC eq "libx264") {
	@args = ("ffmpeg", "-r", "$FPS", "-i", "$TEMP/frame_%7d$SUFFIX", "-r", "$FPS", "-vcodec", "libx264", "-vpre", "hq", "-y", "$OUTFILE");
} else {
	@args = ("ffmpeg", "-r", "$FPS", "-i", "$TEMP/frame_%7d$SUFFIX", "-r", "$FPS", "-vcodec", "$VCODEC", "-y", "$OUTFILE");
#	@args = ("mencoder", "mf://$TEMP/*$SUFFIX", "-mf","fps=$FPS", "-mf", "type=jpg", "-o","$OUTFILE", "-ovc","lavc", "-lavcopts","vcodec=$VCODEC");
}
system(@args);

if (1) {
	print "cleaning up $TEMP\n";
	foreach (@files) {
		unlink ("$TEMP/$_$DELSFX");
	}
	rmdir $TEMP
}

system("ls -l $OUTFILE");
