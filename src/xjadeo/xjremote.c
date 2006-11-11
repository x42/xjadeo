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
#include <bindir.h>

#ifndef HAVE_MQ 

int main() {
	printf("This xjadeo was compiled without POSIX mqueue messages\n");
	printf("resort to 'xjadeo -R' stdio remote interface.\n");
	exit(1);
	// TODO : make a busybox or exec 'xjadeo -R'??
}

#else  /* HAVE_MQ */

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
 * currently this acts a dumb stdio wrapper 
 * with a maxmsg of 255 bytes.
 *
 * sooner or later remote.c commands will be parsed here
 * (readline) and xjadeo will drop the text-remote-ctl in favor of
 * fast message queues..
 * */
typedef struct {
	int cmd;
	char m[MQLEN];
} mqmsg;

int loop_flag  = 1;

void *read_thread (void *d) {
	mqd_t           mqfd;
	char           *msg_buffer;
	struct mq_attr  mqat;
	char qname[64];
	snprintf(qname,64,"/xjadeo-reply%s%s", d?"-":"", d?(char*)d:"");

	printf("# STARTING reply receiver: mqID:%s\n",d?qname+13:"[default]");

	mqfd = mq_open(qname, O_RDONLY | O_CREAT, S_IRWXU , NULL);
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
		struct timeval tv;
		struct timespec to;
		gettimeofday(&tv,NULL);
		to.tv_sec=tv.tv_sec + 2 ;
		to.tv_nsec= (long) tv.tv_usec * 1000L;

		int num_bytes_received = mq_timedreceive(mqfd, msg_buffer, mqat.mq_msgsize, 0, &to);
		if (num_bytes_received == -1 && errno==ETIMEDOUT) {
			// TODO : ping xjadeo ..? and print a warning.
			continue;
		}
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
		printf ("@%d %s", mymsg->cmd, mymsg->m); // newline is part of data payload.
		if ( mymsg->cmd = 100 && !strncmp(mymsg->m,"quit.",5)) {
			printf("# xjadeo terminated. we will follow.\n");
			loop_flag=0;
		}
	}
	printf ("# SHUTTING DOWN receiver thread.\n");
	printf ("  [press enter to exit]\n"); // XXX
	mq_close(mqfd);
	return (NULL);
}
/*  - might come in handy again.
void select_sleep(int usec) {
	fd_set          fd;
	struct timeval  tv = { 0, 0 };
	tv.tv_sec = 0;
	tv.tv_usec = usec;
	FD_ZERO(&fd);
	select(0, &fd, NULL, NULL, &tv);
}
*/

int testexec (char *filename) {
	struct stat s;
	if (!filename) return (0);
	int result= stat(filename, &s);
	if (result != 0) return 0; /* stat() failed */
	if (S_ISREG(s.st_mode)) return 1; /* is a regular file - ok */
	//TODO check for S_IXUSR|S_IXGRP|S_IXOTH
        return(0); 
}


void forkjadeo (void) {
	int status, died;
	// TODO create remote-mqID and set pass it to xjadeo.
	// check: is there a way to list mq's  apart from mounting /dev/mqueue ??
	printf("# launching a new xjadeo instance for you..\n");
	pid_t pid = fork();
	switch (pid) {
		case  -1: 
			fprintf(stderr,"fork failed\n");
			exit(-1);
		case 0:
			close(0);
			close(1);
			char *xjadeo = getenv("XJADEO");
			if (!testexec(xjadeo)) { xjadeo=NULL; printf("# xjadeo executable not found in : %s\n",xjadeo); }
			if (!xjadeo) xjadeo = BINDIR "/xjadeo";
			if (!testexec(xjadeo)) { xjadeo=NULL; printf("# xjadeo executable not found in : %s\n",xjadeo); }
			xjadeo = "src/xjadeo/xjadeo";
			if (!testexec(xjadeo)) { xjadeo=NULL; printf("# xjadeo executable not found in : %s\n",xjadeo); }
			xjadeo = "../xjadeo/xjadeo";
			if (!testexec(xjadeo)) { xjadeo=NULL; printf("# xjadeo executable not found in : %s\n",xjadeo); }

			if (xjadeo) execl(xjadeo,"xjadeo", "-R", 0);
			//if (xjadeo) execl(xjadeo,"xjadeo", "-R", "-q", 0);
			exit(0);
		default:
			fprintf(stdout,"# started xjadeo.\n");
	}
}

int main(int argc, char **argv) {
	int             status = 0;
	mqd_t           mqfd_tx = -1;
	int             num_bytes_to_send;
	int             priority_of_msg;
	pthread_t       xet;
	char qname[64];
	int did_fork = 0;

	// TODO: parse Command line argv
	
	int want_nofork = 0;
	int want_create = 0;
	char *qarg = NULL; 

	snprintf(qname,64,"/xjadeo-request%s%s", qarg?"-":"", qarg?qarg:"");

	printf("# initializing mqID:%s\n",qarg?qname+15:"[default]");

	do {
		mqfd_tx = mq_open(qname, O_WRONLY | (want_create?O_CREAT:0), S_IRWXU , NULL);
		if (mqfd_tx < 0) {
			printf("# could not connect to a xjadeo remote.\n");
			if (want_nofork) exit (0);
			if (!did_fork) { forkjadeo(); did_fork=1;}
			sleep (1);
			// cleanup - option ??
			// in case there are queues but xjadeo's dead // (ping!)
		}
	} while (mqfd_tx < 0);

	pthread_create(&xet, NULL, read_thread, NULL);

	if (mqfd_tx == -1) {
		perror("mq_open failure from main");
		exit(0);
	};

	priority_of_msg = 20;
	printf("# COMMAND INTERFACE ACTIVATED: use 'exit' or EOF to terminate this session.\n");
	while (loop_flag) {
		char buf[MQLEN];
		// TODO: use select  and read() XXX
		if(!fgets(buf,MQLEN,stdin)) { 
		#if 1
			loop_flag=0;
			break;
		#else
			usleep(100000);
			continue;
		#endif
		}

		if (!strncmp(buf,"exit",4)) {
			loop_flag=0;
			break;
		}

		mqmsg mymsg = {1, "" };
		strncpy(mymsg.m,buf,MQLEN);
		num_bytes_to_send = sizeof(mqmsg);

		status = mq_send(mqfd_tx, (char*) &mymsg, num_bytes_to_send, priority_of_msg);
		if (status == -1) {
			perror("mq_send failure on mqfd_tx");
			continue;
		}
	//	printf ("# successful call to mq_send, i = %d\n", i);
	}


	loop_flag=0; // stop read thread.
	printf("bye bye.\n");
	pthread_join(xet,NULL);

	if (mq_close(mqfd_tx) == -1)
		perror("mq_close failure on mqfd_tx");

	return (0);
}
#endif
