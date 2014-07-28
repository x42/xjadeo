/* xjremote - remote control master
 *
 * Copyright (C) 2006,2014 Robin Gareus
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#define EXIT_FAILURE 1

/* we'll use mq_timedreceive */
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <getopt.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "paths.h"

static void usage (int status);
static void printversion (void);

static char *program_name;
static int want_quiet      = 0;    /*< --quiet, --silent */
static int want_verbose    = 0;    /*< --verbose */
static int want_ping       = 1;    /*< --noping */
static int want_unlink     = 0;    /*< --1:unlink mqueues on startup  2: and exit */
static int want_nofork     = 0;    /*< --nofork ; donT launch xjadeo */
static char *qid           = NULL; /*< -I <arg> - name of the MQ */
static int want_create     = 0;    /*< unused - only xjadeo create queues */
static int no_initial_sync =0;     /* --nosyncsource, -J */

static int xjr_mute     = 1;       /*< 1: mute all but '8xx' messages
                                    *  2: dont display  any replies
                                    *  0: terminal mode */


static struct option const long_options[] =
{
	{"nofork",            no_argument, 0,       'f'},
	{"help",              no_argument, 0,       'h'},
	{"id",                required_argument, 0, 'I'},
	{"no-initial-sync",   no_argument, 0,       'J'},
	{"no-ping",           required_argument, 0, 'P'},
	{"quiet",             no_argument, 0,       'q'},
	{"silent",            no_argument, 0,       'q'},
	{"unlinkonly",        no_argument, 0,       'U'},
	{"unlink",            no_argument, 0,       'u'},
	{"version",           no_argument, 0,       'V'},
	{"verbose",           no_argument, 0,       'v'},
	{NULL, 0, NULL, 0}
};

static int decode_switches (int argc, char **argv) {
	int c;
	while ((c = getopt_long (argc, argv,
			   "f"  /* nofork */
			   "h"  /* help */
			   "I:" /* queue id */
			   "J"  /* no jack / no-initial sync */
			   "P"  /* noping */
			   "Q"  /* remote - arg for xjadeo compatibilty */
			   "q"  /* quiet or silent */
			   "R"  /* remote - arg for xjadeo compatibilty */
			   "U"  /* unlinkonly */
			   "u"  /* unlink */
			   "V"  /* version */
			   "v"  /* verbose */
			   "W:" /* remote - arg for xjadeo compatibilty */
			   , long_options, (int *) 0)) != EOF)
	{ switch (c) {
		case 'f':
			want_nofork = 1;
			break;
		case 'h':
			usage (0);
			break;
		case 'I':
			if (qid) free(qid);
			qid = strdup(optarg);
			break;
		case 'J':
			no_initial_sync = 1;
			break;
		case 'P':
			want_ping = 0;
			break;
		case 'Q':
			break;
		case 'q':
			want_quiet = 1;
			want_verbose = 0;
			break;
		case 'R':
			break;
		case 'U':
			want_unlink = 2;
			break;
		case 'u':
			want_unlink = 1;
			break;
		case 'V':
			printversion();
			exit(0);
			break;
		case 'v':
			want_verbose = 1;
			break;
		case 'W':
			break;
		default:
			usage (EXIT_FAILURE);
			break;
	} } /* while switch */
	return optind;
}

static void usage (int status) {
	printf ("xjremote - jack video monitor remote control utility\n\n");
	printf ("Usage: xjremote[ OPTIONS ]\n\n");
  printf ("\n\n"
"Xjremote opens a connection to a running instance of xjadeo or launches a new\n"
"instance ready for remote-control.\n"
"\n"
"Interaction with xjremote takes place via standard input/output mechanism,\n"
"while xjremote itself communicate with xjadeo out-of-band using platform\n"
"specific operations (e.g. POSIX message queues, IPC or OSC).\n"
"\n"
/*-------------------------------------------------------------------------------|" */
"Options:\n"
" -h, --help                display this help and exit\n"
" -V, --version             print version information and exit\n"
" -f, --nofork              connect only to already running instances and\n"
"                           do NOT launch a new xjadeo if none found.\n"
" -P, --noping              do not check if xjadeo is alive. just connect.\n"
" -q, --quiet, --silent     inhibit usual output\n"
" -I, --id <queue-id>       specify queue id.\n"
" -J, --no-initial-sync     passed on to xjadeo as option.\n"
" -u, --unlink              remove existing queues\n"
" -U, --unlinkonly          remove queues and exit\n"
"                           active connections will not be affected by an\n"
"                           unlink event.\n"
"\n"
"xjemote also accepts -R, -Q, and -W options to be compatible with xjadeo\n"
"as a drop-in-replacement. Those options are ignored by xjremote.\n"
"\n"
"Report bugs to Robin Gareus <robin@gareus.org>\n"
"Website: <https://github.com/x42/xjadeo>\n"
"\n");
	exit (status);
}

static void printversion (void) {
	printf ("xjremote version %s\n\n", VERSION);
#ifdef SUBVERSION
	if (strlen(SUBVERSION)>0 && strcmp(SUBVERSION, "exported")) {
		printf (" built from:     scm-%s\n", SUBVERSION);
	}
#endif
#ifdef HAVE_MQ
	printf (" configuration:  [ POSIX-MQueue ]\n");
#elif HAVE_IPCMSG
	printf (" configuration:  [ IPC-MSG ]\n");
#else
	printf (" configuration:  [ stdio ]\n");
#endif
	printf (
			"Copyright (C) GPL 2006,2014 Robin Gareus <robin@gareus.org>\n"
			"This is free software; see the source for copying conditions.  There is NO\n"
			"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
}

/* check if file is executable */
static int testexec (char *filename) {
	struct stat s;
	if (!filename) return (0);
	int result= stat(filename, &s);
	if (result != 0) return 0; /* stat() failed */
#ifdef PLATFORM_WINDOWS
	return(1);
#else
	if (!S_ISREG(s.st_mode) && !S_ISLNK(s.st_mode)) return 0; /* is not a regular file */
	if (s.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))  return 1; /* is executable */
	return(0);
#endif
}

// flags: bit0 (1): close stdio
//        bit1 (2): fork xjadeo -R (otherwise '-Q' if bit2 is unset)
//        bit2 (4): fork xjadeo -W queuefile
void execjadeo(int flags, char *queuefile) {
	char *xjadeo = getenv("XJADEO");
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }
	if (!xjadeo) xjadeo = BINDIR "xjadeo";
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }
	if (!xjadeo) xjadeo = "/Applications/Jadeo.app/Contents/MacOS/Jadeo"; // OSX
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }
	if (!xjadeo) xjadeo = "./xjadeo"; // XXX DEVEL svn:trunk/src/xjadeo
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }
	if (!xjadeo) xjadeo = "src/xjadeo/xjadeo"; // XXX DEVEL svn:trunk/
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }
	if (!xjadeo) xjadeo = "../xjadeo/xjadeo"; // XXX DEVEL svn:trunk/src/qt-qui
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }
	if (!xjadeo) xjadeo = "./bin/xjadeo"; // XXX ~/ -> use env("HOME")
	if (!testexec(xjadeo)) { printf("# xjadeo executable not found in : %s\n",xjadeo?xjadeo:"(?)"); xjadeo=NULL; }

	if (xjadeo) {
		printf("# executing: %s\n",xjadeo);
		if (flags&1) {
			close(0);
			dup2(open("/dev/null", 0), 1);
			dup2(open("/dev/null", 0), 2);
		}
		if (no_initial_sync) {
			if (flags&4)
				execl(xjadeo,"xjadeo", "-J", "-q", "-W", queuefile, NULL);
			else if (flags&2)
				execl(xjadeo,"xjadeo", "-J", "-R", NULL);
			else
				execl(xjadeo,"xjadeo", "-J", "-Q", "-q", NULL);
		} else {
			if (flags&4)
				execl(xjadeo,"xjadeo", "-q", "-W", queuefile, NULL);
			else if (flags&2)
				execl(xjadeo,"xjadeo", "-R", NULL);
			else
				execl(xjadeo,"xjadeo", "-Q", "-q", NULL);
		}
	} else {
		printf("# no xjadeo executable found. try to set the XJADEO env. variable\n");
	}
}
#ifdef PLATFORM_WINDOWS
#else
void forkjadeo (void) {
	// TODO create remote-mqID and set pass it to xjadeo.
	// check: is there a way to list mq's  apart from mounting /dev/mqueue ??
	printf("# launching a new xjadeo instance for you..\n");
	pid_t pid = fork();
	switch (pid) {
		case  -1:
			fprintf(stderr,"fork failed\n");
			exit(-1);
		case 0:
			execjadeo(1, NULL);
			fprintf(stdout,"# exec failed.\n");
			exit(0);
		default:
			fprintf(stdout,"# connecting to xjadeo...\n");
	}
}
#endif

//-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <sys/resource.h>
#endif
#include <time.h>
#define REMOTE_RX fileno(stdin)

int xjselect (int sec) {
#ifdef PLATFORM_WINDOWS
	DWORD bytesAvail = 0;
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	PeekNamedPipe(h, 0, 0, 0, &bytesAvail, 0);
	if (bytesAvail > 0) return (1);
	return(0);
#else
	fd_set fd;
	int max_fd=0;
	struct timeval tv = { 0, 0 };
	tv.tv_sec = sec; tv.tv_usec = 0;

	FD_ZERO(&fd);
	FD_SET(REMOTE_RX,&fd);
	max_fd=(REMOTE_RX+1);
	if (select(max_fd, &fd, NULL, NULL, &tv)) return(1);
	return(0);
#endif
}

//-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#

#ifdef HAVE_MQ

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
int loop_flag = 1;
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
			timeout_cnt++;
			continue;
		}
		if (num_bytes_received == -1) {
			perror("mq_receive failure on mqfd");
#if 0
			usleep(40000);
			continue;
#else
			loop_flag = 0;
			break;
#endif
		}
		if (num_bytes_received != sizeof(mqmsg) )  {
			if (!want_quiet)  // display anyway ? warning ??
				fprintf(stderr,"MQ: received garbage message\n");
			continue;
		}
		timeout_cnt=0;
		printit = !xjr_mute;

		mqmsg *mymsg = (mqmsg*) &msg_buffer[0];
		if ( xjr_mute==0 && mymsg->cmd == 100 && !strncmp(mymsg->m,"quit.",5)) {
			if (want_verbose)
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
			(void) write(REMOTE_TX,tmp,strlen(tmp));
		#else
			printf ("@%d %s", mymsg->cmd, mymsg->m); // newline is part of data payload.
			fflush(stdout);
		#endif
		}
	}
	if (want_verbose)
		printf ("# SHUTTING DOWN receiver thread.\n");
	mq_close(mqfd);
	free(msg_buffer);
	return (NULL);
}

void unlink_queues (char *queueid) {
	char qname[64];
	if (!want_quiet)
		printf("# unlinking queue mqID=%s\n",queueid?qname+15:"[default]");
	snprintf(qname,64,"/xjadeo-request%s%s", queueid?"-":"", queueid?queueid:"");
	mq_unlink(qname);
	snprintf(qname,64,"/xjadeo-reply%s%s", queueid?"-":"", queueid?queueid:"");
	mq_unlink(qname);
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


void dothework (mqd_t mqfd_tx) {
	int             num_bytes_to_send;
	int             priority_of_msg = 20;

	mqmsg mymsg = {1, "" };
	num_bytes_to_send = sizeof(mqmsg);

	if (!want_quiet) {
		printf("# COMMAND INTERFACE ACTIVATED: use 'exit' or EOF to terminate this session.\n");
		printf("#  use 'quit' to terminate xjadeo and disconnect.\n");
		printf("#  type 'help' to query xjadeo commands.\n");
	}

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
			loop_flag=0;
			break;
		}

		while ((end = strchr(buf, '\n'))) {
			int retry = 10;
			*(end) = '\0';

			if (!strncmp(buf,"exit",4)) {
				loop_flag=0;
				break;
			}
			snprintf(mymsg.m,MQLEN,"%s\n",buf);
			//strncpy(mymsg.m,buf,MQLEN-1); // add '\n'
			mymsg.m[MQLEN-1]=0;

			while (--retry && mq_send(mqfd_tx, (char*) &mymsg, num_bytes_to_send, priority_of_msg) == -1) {
				usleep(50000);
			}
			if (!retry) {
				perror("mq_send failure on mqfd_tx");
				// ping ?
			}

			offset-=((++end)-buf);
			if (offset) memmove(buf,end,offset);
		}
	}
}

int main (int argc, char **argv) {
	int 		i;
	pthread_t       xet;
	mqd_t		mqfd_tx;
	char		qname[64];
	int		did_fork = 0;
	int		timeout;

	program_name = argv[0];

	i = decode_switches (argc, argv);

	if ((i)!= argc) usage (EXIT_FAILURE);

	if (want_unlink) unlink_queues(qid);
	if (want_unlink&2) exit(0);

restart:
	/* set up outgoing connection first */
	loop_flag=1;
	timeout = 10;

	snprintf(qname,64,"/xjadeo-request%s%s", qid?"-":"", qid?qid:"");
	if (want_verbose)
		printf("# initializing mqID=%s\n",qid?qname+15:"[default]");

	do {
		mqfd_tx = mq_open(qname, O_WRONLY | O_NONBLOCK | (want_create?O_CREAT|O_EXCL:0), S_IRWXU , NULL);
		if (mqfd_tx < 0) {
			if ( errno != ENOENT ) break;
			if (!want_quiet)
				printf("# could not connect to xjadeo. still trying.\n");
			if (want_nofork) {
				if (!want_quiet)
					printf("# giving up. There's no running instance of xjadeo with MQ enabled.\n");
				exit (0);
			}
			if (!did_fork) { forkjadeo(); did_fork=1;}
			sleep (1);
		}
	} while (mqfd_tx < 0 && timeout--);

	if (mqfd_tx == -1) {
		perror("mq_open failure.");
		exit(0);
	};

	/* create incoming connection + handler */
	pthread_create(&xet, NULL, read_thread, NULL);

	/* ping xjadeo - alive check */
	if (want_ping) {
		if (!want_quiet)
			fprintf(stdout, "# pinging xjadeo...\n");
		// TODO flush the queue before pinging
		ping(mqfd_tx);
		timeout=50; // 5 sec
		while(!pong_st && --timeout) {
			usleep(100000);
		}
		if (!timeout) {
			if (!want_quiet)
				fprintf(stdout, "# WARNING: queues exist, but xjadeo does not respond\n");
			if (want_verbose)
				fprintf(stdout, "# deleting stale message queues.\n");
#if 1
			if (!did_fork) {
				mq_close(mqfd_tx);
				loop_flag=0; // stop read thread.
				pthread_join(xet,NULL);
				ping_st=0;pong_st=0;
				unlink_queues(qid);
				goto restart;
			} else
#endif
			unlink_queues(qid);
			if (!want_quiet)
				fprintf(stdout, "# please try again.\n");
			exit(1);
		}
	}

	xjr_mute = 0;
	dothework(mqfd_tx);

	/* time to go */
	loop_flag=0; // stop read thread.
	if (!want_quiet) printf("bye bye.\n");
	pthread_join(xet,NULL);

	if (mq_close(mqfd_tx) == -1)
		perror("mq_close failure on mqfd_tx");

	if (qid) free(qid);
	return (0);
}

#elif HAVE_IPCMSG

#include <pthread.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <errno.h>

struct msgbuf1 {
	long    mtype;
	char    mtext[BUFSIZ];
};

int loop_flag = 1;
int unlink_queue_on_exit = 1;

void *rx_thread (void *arg) {
	int i, rv, msqid;
	msqid = *((int*)arg);
	struct msgbuf1 rxbuf;

  while (loop_flag) {
		rv = msgrcv(msqid, (void*) &rxbuf, BUFSIZ, 1, 0);

		if(rv == -1)  {
			fprintf(stderr, "\nCTL: Msgrcv failed., Error = %d: %s\n", errno, strerror(errno));
			pthread_exit(0);
			return 0;
		}

		for(i = 0; i<rv; i++)
			putchar(rxbuf.mtext[i]);
		fflush(stdout);

		if (!strncmp(rxbuf.mtext,"@100 quit.",9)) {
			if (want_verbose)
				printf("# xjadeo terminated. we will follow.\n");
			loop_flag=0;
		}
	}
	pthread_exit(0);
	return 0;
}

void tx_loop(int msqid) {

  char buf[BUFSIZ];
  int offset =0;
	struct msgbuf1 txbuf;

	txbuf.mtype = 1;

  while (loop_flag) {
    int rx;
    char *end;
    if (!xjselect(1)) continue;
    if ((rx = read(REMOTE_RX, buf + offset, (BUFSIZ-1)-offset)) > 0) {
      offset += rx;
      buf[offset] = '\0';
    } else if (rx < 0) {
      continue;
    } else {
			if (loop_flag) unlink_queue_on_exit=0;
      loop_flag=0;
      break;
    }

		while ((end = strchr(buf, '\n'))) {
      int retry = 10;
      *(end) = '\0';

      if (!strncmp(buf,"exit",4)) {
			  unlink_queue_on_exit=0;
        loop_flag=0;
        break;
      }
      snprintf(txbuf.mtext,BUFSIZ,"%s\n",buf);
      //strncpy(txbuf.mtext,buf,BUFSIZ-1); // add '\n' and '\0'
      txbuf.mtext[BUFSIZ-1]=0;

      while (--retry && msgsnd(msqid, (const void*) &txbuf, strlen(txbuf.mtext), IPC_NOWAIT) == -1) {
        usleep(50000);
      }
      if (!retry) {
				fprintf(stderr, "CTL: msgsnd failed. Error = %d: %s\n", errno, strerror(errno));
      }

      offset-=((++end)-buf);
      if (offset) memmove(buf,end,offset);
    }
  }
}

int main (int argc, char **argv) {

	int msqrx, msqtx;
	pthread_t  xet;
	char *queuename = NULL;

	program_name = argv[0];
	int i = decode_switches (argc, argv);

	if ((i)!= argc) usage (EXIT_FAILURE);

	if (qid) queuename = strdup(qid);
	else queuename = tempnam("/tmp", "xjremote");

	if (want_unlink) unlink (queuename);
	if (want_unlink&2) exit(0);

	int fd = open (queuename, O_WRONLY | O_CREAT | O_NONBLOCK | O_NOCTTY, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		fprintf(stderr, "\nCan not create queue. error = %d: %s\n", errno, strerror(errno));
		return -1;
	}

	printf("@ IPC Queue name: %s\n", queuename);

	if (!want_nofork) {
		if (!want_quiet)
			printf("# launching a new xjadeo instance for you..\n");
		pid_t pid = fork();
		if (pid == -1) return -2;
		else if (pid == 0) {
			execjadeo(5, queuename);
			fprintf(stderr,"CTL: EXEC failed\n");
			exit (1);
		}
	}

  key_t key_rx = ftok (queuename, 'a');
  key_t key_tx = ftok (queuename, 'b');

	msqrx = msgget(key_rx, IPC_CREAT| S_IRUSR | S_IWUSR);
	msqtx = msgget(key_tx, IPC_CREAT| S_IRUSR | S_IWUSR);
	if(msqrx == -1 || msqtx == -1)  {
		printf("CTL: getKey failed. Error = %d: %s\n", errno, strerror(errno));
		return -1;
	}
	// TODO: try ping ?!
	loop_flag=1;

	pthread_create(&xet, NULL, rx_thread, (void*) &msqrx);

	if (!want_quiet) {
		printf("# COMMAND INTERFACE ACTIVATED: use 'exit' or EOF to terminate this session.\n");
		printf("# use 'quit' to terminate xjadeo and disconnect.\n");
		printf("# type 'help' to query xjadeo commands.\n");
	}

  tx_loop(msqtx);

  loop_flag=0; // stop read thread.
  if (!want_quiet) printf("bye bye.\n");
  pthread_cancel(xet);
  pthread_join(xet,NULL);

	if (unlink_queue_on_exit) {
		msgctl(msqtx, IPC_RMID, NULL);
		msgctl(msqrx, IPC_RMID, NULL);
		unlink (queuename);
	}
	else if (!want_quiet)
		printf("# keeping IPC queue: '%s'\n# resume with %s -f -I %s\n", queuename, program_name, queuename);

	free(queuename);
	if (qid) free(qid);

	return 0;
}

#else
int main(int argc, char **argv) {
	program_name = argv[0];
	decode_switches (argc, argv);
	printf("# This xjadeo was compiled without POSIX-mqueue and IPC messages.\n");
	if (want_nofork) exit (0);
	printf("# -> stdio remote terminal.\n");
	execjadeo(2, NULL);
	exit(1);
}
#endif
