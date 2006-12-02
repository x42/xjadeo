#!/usr/bin/perl

$OUTFILE="/tmp/test.avi";
$VCODEC="mpeg4";

$FPS="25";
#$FPS="29.97";
#$FPS="30";
#$FPS="24";
$DURATION="1:05:00"; 



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

if (1) {
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
			system("convert $TEMP/$frame  $TEMP/$frame.jpg");
			if ($? != 0) { die "failed to execute: $!\n"; }
			unlink ("$TEMP/$frame");
			$i++;
		}
	}
}

if (1) {
	print "\n3) encoding jpg -> $OUTFILE\n";
	my @args = ("mencoder", "mf://$TEMP/*.jpg", "-mf","fps=$FPS", "-mf", "type=jpg", "-o","$OUTFILE", "-ovc","lavc", "-lavcopts","vcodec=$VCODEC");
	system(@args);

}

if (1) {
	print "cleaning up $TEMP\n";
	foreach (@files) {
		unlink ("$TEMP/$_.jpg");
	}
	rmdir $TEMP
}
