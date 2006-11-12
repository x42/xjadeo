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

    $Rev:$ 
*/
#define EXIT_FAILURE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bindir.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <getopt.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
static void usage (int status);
static void printversion (void);

char *program_name;
int want_quiet   = 0;	/*< --quiet, --silent */
int want_debug   = 0;	/*< --debug */
int want_verbose = 0;	/*< --verbose */
int want_nofork  = 0;	/*< --nofork ; donT launch xjadeo */
char *qid	 = NULL;  /*< -I <arg> - name of the MQ */
int want_create  = 0;	/*< unused - only xjadeo create queues */ 

int xjr_mute 	 = 1;	/*< 1: mute all but '8xx' messages
			 *  2: dont display  any replies
			 *  0: terminal mode */
	

static struct option const long_options[] =
{
	{"quiet", no_argument, 0, 'q'},
	{"silent", no_argument, 0, 'q'},
	{"verbose", no_argument, 0, 'v'},
	{"nofork", no_argument, 0, 'f'},
	{"id", required_argument, 0, 'I'},
	{"debug", no_argument, 0, 'D'},
	{NULL, 0, NULL, 0}
};

static int decode_switches (int argc, char **argv) {
	int c;
	while ((c = getopt_long (argc, argv, 
			   "q"	/* quiet or silent */
			   "v"	/* verbose */
			   "h"	/* help */
			   "R"	/* remote - arg retained for xjadeo compatibilty */
			   "I:"	/* queue id */
			   "f"	/* nofork */
			   "V",	/* version */
			   long_options, (int *) 0)) != EOF)
	{ switch (c) {
		case 'q':		/* --quiet, --silent */
			want_quiet = 1;
			want_verbose = 0;
			break;
		case 'v':		/* --verbose */
			want_verbose = 0;
			break;
		case 'I':		/* --id */
			qid=strlen(optarg)?strdup(optarg):NULL;
			break;
		case 'f': 		/* --nofork */
			want_nofork = 1;
			break;
		case 'R':
			break;
		case 'V':
			printversion();
			exit(0);
			break;
		case 'h':
			usage (0);
		default:
			usage (EXIT_FAILURE);
	} } /* while switch */
	return optind;
}


static void usage (int status) {
	printf ("%s -  jack video monitor remote control utility\n", program_name);
	printf ("usage: %s [Options]\n", program_name);
	printf (""
"Options:"
"  -h, --help                display this help and exit\n"
"  -V, --version             print version information and exit\n"
"  -f, --nofork              connect only to already running instances and\n"
"                            do not launch a new xjadeo if none found.\n"
//"  -I <val>, --id <val>    \n"
"  \n"
);

  exit (status);
}

static void printversion (void) {
	printf ("xjremote version %s\n", VERSION);
}

/* check if file is executable */
int testexec (char *filename) {
	struct stat s;
	if (!filename) return (0);
	int result= stat(filename, &s);
	if (result != 0) return 0; /* stat() failed */
	if (!S_ISREG(s.st_mode)) return 0; /* is not a regular file */
	if (s.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))  return 1; /* is executable */
        return(0); 
}

void execjadeo(int closestdio) {
	char *xjadeo = getenv("XJADEO");
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }
	if (!xjadeo) xjadeo = BINDIR "/xjadeo";
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }
	xjadeo = "./xjadeo"; // XXX DEVEL svn:trunk/src/xjadeo
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }
	xjadeo = "src/xjadeo/xjadeo"; // XXX DEVEL svn:trunk/
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }
	xjadeo = "../xjadeo/xjadeo"; // XXX DEVEL svn:trunk/src/qt-qui
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }

	if (xjadeo) {
		printf("# exec: %s\n",xjadeo);
		if (closestdio) { close(0); close(1);}
		execl(xjadeo,"xjadeo", "-R", "-q", 0);
		//execl(xjadeo,"xjadeo", "-R", 0);
	}
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
			execjadeo(1);
			exit(0);
		default:
			fprintf(stdout,"# started xjadeo.\n");
	}
}

//-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#

#ifndef HAVE_MQ 

int main(int argc, char **argv) {
	program_name = argv[0];
	printf("# This xjadeo was compiled without POSIX mqueue messages.\n");
	printf("# -> stdio remote terminal.\n");
	execjadeo(0);
	exit(1);
	// TODO : make a busybox or exec 'xjadeo -R'??
}

#else  /* HAVE_MQ */

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

// globals shared between threads 
int loop_flag  = 1;
int ping_st =0;
int pong_st =0;
struct timeval ping_time,pong_time;

#define REMOTE_TX fileno(stdout)
void *read_thread (void *d) {
	mqd_t           mqfd;
	char           *msg_buffer;
	int		timeout_cnt = 0;
	struct mq_attr  mqat;
	char		qname[64];

	snprintf(qname,64,"/xjadeo-reply%s%s", d?"-":"", d?(char*)d:"");

	printf("# STARTING reply receiver: mqID=%s\n",d?qname+13:"[default]");

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
		int printit;
		struct timeval tv;
		struct timespec to;
		gettimeofday(&tv,NULL);
		to.tv_sec=tv.tv_sec + 2 ;
		to.tv_nsec= (long) tv.tv_usec * 1000L;
#if 0
		if (timeout_cnt == 25 ) {
			ping(.);
		} else if (timeout_cnt > 30 ) {
			timeout=0;
			fprintf(stdout,"# no live signs from xjadeo.\n");
		}
#endif
		int num_bytes_received = mq_timedreceive(mqfd, msg_buffer, mqat.mq_msgsize, 0, &to);
		if (num_bytes_received == -1 && errno==ETIMEDOUT) {
			// TODO : if this happens for a while - ping xjadeo ..? and print a warning.
			timeout_cnt++;
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
		timeout_cnt=0;
		printit = !xjr_mute;
	
		mqmsg *mymsg = (mqmsg*) &msg_buffer[0];
		if ( mymsg->cmd == 100 && !strncmp(mymsg->m,"quit.",5)) {
			printf("# xjadeo terminated. we will follow.\n");
			loop_flag=0;
		} else if ( mymsg->cmd == 100 && !strncmp(mymsg->m,"pong.",5) && ping_st && !pong_st) {
			printit=0; pong_st=1;
			gettimeofday(&pong_time,NULL);
		} else if ( mymsg->cmd/100 == 8 && xjr_mute<2) {
			printit=1;
		}
		if (printit) {
		#if 1
			char tmp[8+MQLEN];
			snprintf (tmp,(MQLEN+8),"@%d %s", mymsg->cmd, mymsg->m); // newline is part of data payload.
 			write(REMOTE_TX,tmp,strlen(tmp));
		#else
			printf ("@%d %s", mymsg->cmd, mymsg->m); // newline is part of data payload.
			fflush(stdout);
		#endif
		}
	}
	printf ("# SHUTTING DOWN receiver thread.\n");
	mq_close(mqfd);
	return (NULL);
}

void ping (mqd_t mqfd_tx) {
	if (ping_st) return;
	ping_st=1; pong_st=0;
	gettimeofday(&ping_time,NULL);
	// send 'ping'
	int             num_bytes_to_send;
	int             priority_of_msg = 20;

	mqmsg mymsg = {1, "ping\n" };
	num_bytes_to_send = sizeof(mqmsg);

	if(mq_send(mqfd_tx, (char*) &mymsg, num_bytes_to_send, priority_of_msg) == -1) {
		perror("mq_send failure on mqfd_tx");
	}
}

#define REMOTE_RX fileno(stdin) 

int xjselect (int sec) {
	fd_set fd;
	int max_fd=0;
	struct timeval tv = { 0, 0 };
	tv.tv_sec = sec; tv.tv_usec = 0;

	FD_ZERO(&fd);
	FD_SET(REMOTE_RX,&fd);
	max_fd=(REMOTE_RX+1);
	if (select(max_fd, &fd, NULL, NULL, &tv)) return(1);
	return(0);
}



void dothework (mqd_t mqfd_tx) {
	int             num_bytes_to_send;
	int             priority_of_msg = 20;

	mqmsg mymsg = {1, "" };
	num_bytes_to_send = sizeof(mqmsg);

	printf("# COMMAND INTERFACE ACTIVATED: use 'exit' or EOF to terminate this session.\n");

	char buf[MQLEN];
	int offset =0;

	while (loop_flag) {
		int rx;
		char *end;
		if (!xjselect(1)) continue;
		if ((rx = read(REMOTE_RX, buf + offset, (MQLEN-1)-offset)) > 0) {
			offset += rx;	
			buf[offset] = '\0';
		} else if (rx < 0) {
			continue;
		} else {
			continue;
			loop_flag=0;
			break;
		}

		while ((end = strchr(buf, '\n'))) {
			int retry = 5;
			*(end) = '\0';

			if (!strncmp(buf,"exit",4)) {
				loop_flag=0;
				break;
			}
			snprintf(mymsg.m,MQLEN,"%s\n",buf);
			//strncpy(mymsg.m,buf,MQLEN-1); // add '\n'
			mymsg.m[MQLEN-1]=0;

			while (--retry && mq_send(mqfd_tx, (char*) &mymsg, num_bytes_to_send, priority_of_msg) == -1) {
				sleep(1);
			}
			if (!retry) {
				perror("mq_send failure on mqfd_tx");
			}
			
			offset-=((++end)-buf);
			if (offset) memmove(buf,end,offset);
		}
	}
}

int main(int argc, char **argv) {
	int 		i;
	pthread_t       xet;
	mqd_t		mqfd_tx;
	char		qname[64];
	int		did_fork = 0;
	int		timeout;

	program_name = argv[0];

	i = decode_switches (argc, argv);

	snprintf(qname,64,"/xjadeo-request%s%s", qid?"-":"", qid?qid:"");

	printf("# initializing mqID=%s\n",qid?qname+15:"[default]");
	timeout = 10;

	do {
		mqfd_tx = mq_open(qname, O_WRONLY | O_NONBLOCK | (want_create?O_CREAT|O_EXCL:0), S_IRWXU , NULL);
		if (mqfd_tx < 0) {
			if ( errno != ENOENT ) break;
			printf("# could not connect to xjadeo.\n");
			if (want_nofork) exit (0);
			if (!did_fork) { forkjadeo(); did_fork=1;}
			sleep (1);
		}
	} while (mqfd_tx < 0 && timeout--);

	if (mqfd_tx == -1) {
		perror("mq_open failure.");
		exit(0);
	};

	pthread_create(&xet, NULL, read_thread, NULL);

	if (1) {
		if (!want_quiet)
			fprintf(stdout, "# pinging xjadeo...\n");
		// TODO flush the queue before pinging
		ping(mqfd_tx);
		timeout=50; // 5 sec
		while(!pong_st && --timeout) {
			usleep(100000);
		}
		if (!timeout) {
			fprintf(stdout, "# WARNING: queues exist, but xjadeo does not respond\n");
			fprintf(stdout, "# unlinking message queues.\n");
			mq_unlink(qname);
			snprintf(qname,64,"/xjadeo-reply%s%s", qid?"-":"", qid?qid:"");
			mq_unlink(qname);
			exit(1);
		}
	}

	xjr_mute = 0;
			
	dothework(mqfd_tx);

	loop_flag=0; // stop read thread.
	printf("bye bye.\n");
	pthread_join(xet,NULL);

	if (mq_close(mqfd_tx) == -1)
		perror("mq_close failure on mqfd_tx");

	if (qid) free(qid);
	return (0);
}
#endif
