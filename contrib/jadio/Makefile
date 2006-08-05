all: jadio 

jadio: jadio.c jack.c Makefile
	gcc -g -O3 -o jadio \
		-I/usr/include/lqt/ -lquicktimehv \
		-L/usr/lib -lavformat -lavcodec \
		-ljack -lsndfile -lsamplerate -lm\
		-Wall jadio.c jack.c

#		-I/usr/include/lqt/ -lquicktime \
#		-I/usr/include/lqt/ -lquicktimehv \

.PHONY: clean tags all
tags:
	ctags *.[hc]
clean:
	-rm jadio tags
