/*  xj-five - message queues

    Copyright (C) 2006 Robin Gareus
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: xj-five.c,v 1.12 2006/09/09 10:55:12 rgareus Exp $
*/
#include <config.h>

//#ifndef HAVE_MQ 
#if 1

int main() {
//	printf("this xjadeo was compiled without POSIX mqueue messages\n");
	printf(" ** xjadeo does not yet support POSIX message queues\n");
	printf(" ** this code is for development purpose only.\n");
	printf(" xjremote is going to a tool that allows to change the\n");
	printf(" running-configuration of xjadeo(1)\n");

	exit(1);
}
#else 

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <string.h>
#include <mqueue.h>
#include <errno.h>
#include <pthread.h>

extern int      errno;

/* xjadeo mq message X-change format
 * 
 * currently this acts a dump stdio with a maxmsg of 255 bytes.
 *
 * sooner or later remote.c stuff will be parsed here
 * and xjadeo will drop the stdio-remote-ctl in favor of
 * fast message queue..
 * */
typedef struct {
	int cmd;
	long a[8];
	char m[256];
} mqmsg;

int loop_flag  = 1;

void *read_thread (void *d) {
	mqd_t           mqfd;
	char           *msg_buffer;
	struct mq_attr  mqat;

	printf("STARTING reply receiver\n");

	mqfd = mq_open("/xjadeo-reply", O_RDONLY | O_CREAT, S_IRWXU | S_IRWXG , NULL);
	if (mqfd == -1) {
		perror("mq_open failure:");
		return(NULL);
	};
	if (mq_getattr(mqfd, &mqat) == -1) {
		perror("mq_getattr error:");
		return(NULL);
	};
	msg_buffer = malloc(mqat.mq_msgsize);

	while (loop_flag) {
		int num_bytes_received = mq_receive(mqfd, msg_buffer, mqat.mq_msgsize, 0);
		if (num_bytes_received == -1) {
			perror("mq_receive failure on mqfd");
			usleep(40000);
			continue;
		}
		if (num_bytes_received != sizeof(mqmsg) )  {
			fprintf(stderr,"MQ: received garbage message\n");
			continue;
		}
	
		mqmsg *mymsg = (mqmsg*) &msg_buffer[0];
		printf ("REPLY: %d = %s \n", mymsg->cmd, mymsg->m);
	}
	printf ("SHUTTING DOWN receiver thread.\n");
	mq_close(mqfd);
	return (NULL);
}

void wake_up_read_thread(void) {
	mqd_t           mqfd = mq_open("/xjadeo-reply", O_WRONLY);
	mq_send(mqfd, "\0", 1, 90);
	mq_close(mqfd);
}


void
select_sleep(int usec) {
	fd_set          fd;
	struct timeval  tv = { 0, 0 };
	tv.tv_sec = 0;
	tv.tv_usec = usec;
	FD_ZERO(&fd);
	select(0, &fd, NULL, NULL, &tv);
}

int main() {
	int             i;
	int             status = 0;
	mqd_t           mqfd_tx;
//	struct mq_attr  mqat;
//	char            msg_buffer[20];
	int             num_bytes_to_send;
	int             priority_of_msg;
	pthread_t       xet;

	printf("INITIALIZING QUEUES\n");

//	pthread_mutex_init(&xev_lock, NULL);
	pthread_create(&xet, NULL, read_thread, NULL);

	mqfd_tx = mq_open("/xjadeo-request", O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG , NULL);
	if (mqfd_tx == -1) {
		perror("mq_open failure from main");
		exit(0);
	};

	priority_of_msg = 20;
	i=0;
	printf("COMMAND INTERFACE ACTIVATED: go and type ahead.\n");
	while (loop_flag) {
		char buf[256];
		if(!fgets(buf,256,stdin)) { 
			fprintf(stderr,"read error.\n");
			sleep(1);
			continue;
		}

		if (!strncmp(buf,"quit",4)) {
			loop_flag=0;
			break;
		}

		// TODO: strip newlines ?!

		mqmsg mymsg = {1, {0,0,0,0,0,0,0,0}, "" };
		strncpy(mymsg.m,buf,256);
		num_bytes_to_send = sizeof(mqmsg);
		mymsg.a[0]=i++;


		status = mq_send(mqfd_tx, (char*) &mymsg, num_bytes_to_send, priority_of_msg);
		if (status == -1) {
			perror("mq_send failure on mqfd_tx");
			continue;
		}
		select_sleep(40000);
	//	printf ("successful call to mq_send, i = %d\n", i);
	}


	loop_flag=0; // stop read thread.
	wake_up_read_thread();
	printf("exiting.\n");
	pthread_join(xet,NULL);

	if (mq_close(mqfd_tx) == -1)
		perror("mq_close failure on mqfd_tx");

	printf("About to exit the sending process after closing the queue \n");
	return (0);
}
#endif
