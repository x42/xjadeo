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
#include "xjadeo.h"

#ifdef HAVE_MQ

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>


#include <mqueue.h>
#include <errno.h>

extern int errno;
extern int loop_run; // display active
extern int loop_flag;// program active


/* see xjremote.c for information on this one */
typedef struct {
	int cmd;
	long a[8];
	char m[256];
} mqmsg;

/* some static globals */
mqd_t           mqfd_r;
mqd_t           mqfd_s;
size_t		mq_msgsize_r;
char		*msg_buffer;
int 		priority_of_msg = 20;



void  mymq_init(void) {
	struct	 	mq_attr  mqat;

	mqfd_r = mq_open("/xjadeo-request", O_RDONLY | O_CREAT| O_NONBLOCK, S_IRWXU | S_IRWXG , NULL);
	if (mqfd_r == -1) {
		perror("mq_open failure:");
		exit(0);
	}
	if (mq_getattr(mqfd_r, &mqat) == -1) {
		perror("mq_getattr error:");
		exit(0);
	}
	mq_msgsize_r = mqat.mq_msgsize;

	mqfd_s = mq_open("/xjadeo-reply", O_WRONLY | O_CREAT| O_NONBLOCK, S_IRWXU | S_IRWXG , NULL);
	if (mqfd_s == -1) {
		// TODO close and unlink mqfd_r
		perror("mq_open failure:");
		exit(0);
	}

	msg_buffer = malloc(mq_msgsize_r);

}

void mymq_close(void) {

	if (mq_close(mqfd_r) == -1)
		perror("mq_close failure on mqfd_r");

	if (mq_close(mqfd_s) == -1)
		perror("mq_close failure on mqfd_s");

	if (mq_unlink("/xjadeo-request") == -1)
		perror("mq_unlink failure");

	if (mq_unlink("/xjadeo-reply") == -1)
		perror("mq_unlink failure");
}

/* read message from queue and store it in data.
 * data needs to be 'NULL' (ignore msg) or point to a valid char[256].
 * return values  0: no message in queue
 *               -1: error
 *               >0: data bytes in message.
 */
int mymq_read(char *data) {
	mqmsg *mymsg;
	int num_bytes_received = mq_receive(mqfd_r, msg_buffer, mq_msgsize_r, 0);

	if (num_bytes_received == -1 && errno==EAGAIN) return (0);
	if (num_bytes_received == -1) {
		perror("mq_receive failure on mqfd");
		usleep(40000);
		return (-1);
	}
	if (num_bytes_received != sizeof(mqmsg) )  {
		fprintf(stderr,"MQ: received garbage message\n");
		return (-1);
	}

	mymsg = (mqmsg*) &msg_buffer[0];
	if (data) strncpy(data,mymsg->m,256);

	return (strlen(data));
}

void mymq_reply(int rv, char *str) {
	mqmsg myrpy = {1, {0,0,0,0,0,0,0,0}, "" };
	myrpy.cmd= rv;
	snprintf(myrpy.m,256,"%s\n",str); // XXX newline
	mq_send(mqfd_s, (char*) &myrpy, sizeof(mqmsg), priority_of_msg);
}

#define LOGLEN 256

/* drop in replacement for remote_printf() */
void mymq_printf(int rv, const char *format, ...) {
	va_list arglist;
	char text[LOGLEN];

	va_start(arglist, format);
	vsnprintf(text, LOGLEN, format, arglist);
	va_end(arglist);

	text[LOGLEN -1] =0; // just to be safe :)
	mymq_reply(rv,text);
}

/* stuff rom remote.c -> move to header file */

typedef void myfunction (void *);
typedef struct _command {
	const char *name;
	const char *help;
	struct _command *children;
	myfunction *func;
	int sticky;  // unused
}  Dcommand;

extern Dcommand *cmd_root;
void exec_remote_cmd_recursive (Dcommand *leave, char *cmd);

/* drop in replacement for remote_read() */
int remote_read_mq(void) {
	int rx;
	char data[256];
	char *t;

	while ((rx=mymq_read(data)) > 0 ) { 
		if ((t =  strchr(data, '\n'))) *t='\0';
		exec_remote_cmd_recursive(cmd_root,data);
	}
	return(0);
}

#if 0

int main(void) {
	int             i;

	mymq_init();

	i=0;
	while (loop_flag) {
		char data[256];
		int num_bytes_received = mymq_read(data);

		if (num_bytes_received < 1) {
			if (num_bytes_received < 0)  perror("mq_receive failure on mqfd");
			else printf("mq: idle.\n");
			usleep (200000);
			continue;
		}
		printf ("received:%i = %s \n", i, data);

		// TODO : decode mymsg - and perform mymsg->cmd action..

		mqmsg *mymsg = (mqmsg*) &msg_buffer[0];

		printf ("data read for iteration %d = %li '%s'\n", mymsg->cmd, mymsg->a[0],mymsg->m);

		mymq_reply(200,mymsg->m);

	//	if (i++ > 20 ) break;
	}

	mymq_close();

	exit(0);
}
#endif

#endif /* HAVE_MQ */
