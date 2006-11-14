#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define XJADEORC "xjadeorc"
#define MAX_LINE_LEN 256 
#define PATH_MAX 255

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <xjadeo.h>
#include "paths.h"

#ifdef SYSCONFDIR
# define SYSCFGDIR SYSCONFDIR
#else
# define SYSCFGDIR "/etc"
#endif

/* test if file exists and is a regular file - returns 1 if ok */
int testfile (char *filename) {
	struct stat s;
	int result= stat(filename, &s);
	if (result != 0) return 0; /* stat() failed */
	if (S_ISREG(s.st_mode)) return 1; /* is a regular file - ok */
        return(0); 
}


#define OPTSTR(arg) if (arg) free(arg); arg=strdup(value); rv=1;

extern char OSD_fontfile[1024]; 
extern double 		delay;
extern int		videomode;
extern int 		seekflags;
extern int want_quiet;
extern int want_verbose;
extern int want_letterbox;
extern int mq_en;
extern int avoid_lash;

int parseoption (char *item, char *value) {
	int rv =0;
	if (!strncasecmp(item,"VIDEOMODE",9)) {
		int vmode; 
		vmode=parsevidoutname(value);
		if (vmode==0 ) vmode = atoi(value);
		if (vmode >=0) {
			videomode = vmode; rv=1;
		}
	} else if (!strncasecmp(item,"FPS",3)) {
		delay = 1.0 / atof(value); rv=1;
	} else if (!strncasecmp(item,"QUIET",7)) {
		if (!strncasecmp(value,"yes",3)){
			want_quiet=1; rv=1;
		}
		else if (!strncasecmp(value,"no",3)) 
			rv=1;
	} else if (!strncasecmp(item,"VERBOSE",7)) {
		if (!strncasecmp(value,"yes",3)){
			want_verbose=1; rv=1;
		}
		else if (!strncasecmp(value,"no",3)) 
			rv=1;
	} else if (!strncasecmp(item,"SEEK",4)) {
		if (!strncasecmp(value,"any",3)){
			seekflags=SEEK_ANY; rv=1;
		} else if (!strncasecmp(value,"cont",4)){
			seekflags=SEEK_CONTINUOUS; rv=1;
		} else if (!strncasecmp(value,"key",3)){
			seekflags=SEEK_KEY; rv=1;
		}
	} else if (!strncasecmp(item,"LETTERBOX",9)) {
		if (!strncasecmp(value,"yes",3)) {
			want_letterbox = 1; rv=1;
		} else if (!strncasecmp(value,"no",2))
			rv=1;
	} else if (!strncasecmp(item,"LASH",4)) {
		if (!strncasecmp(value,"no",2)) {
			avoid_lash = 1; rv=1;
		} else if (!strncasecmp(value,"yes",3))
			rv=1;
	} else if (!strncasecmp(item,"MQ",2)) {
		if (!strncasecmp(value,"yes",3)) {
			mq_en = 1; rv=1;
		} else if (!strncasecmp(value,"no",3)) 
			rv=1;
	} else if (!strncasecmp(item,"FONTFILE",8)) {
		strncpy(OSD_fontfile,value,1023);rv=1;
		OSD_fontfile[1023]=0; // just to be sure.
	}
	return (rv);
}

int readconfig (char *fn) {
       	FILE* config_fp;
       	char line[MAX_LINE_LEN];
       	char* token, *item,*value;
	int lineno=0;
	
       	if (!(config_fp = fopen(fn, "r"))) {
		fprintf(stderr,"configfile failed: %s (%s)\n",fn,strerror(errno));
		return (-1);
	}
#if 0
	fprintf(stdout,"INFO: parsing configfile: %s\n",fn);
#endif
       	while( fgets( line, MAX_LINE_LEN-1, config_fp ) != NULL ) {
		lineno++;
		line[MAX_LINE_LEN-1]=0;
	       	token = strtok( line, "\t =\n\r" ) ; 
		if( token != NULL && token[0] != '#' && token[0] != ';') {
			item=strdup(token);
			token = strtok( NULL, "\t =\n\r" ) ; 
			if (!token) {
				free(item);
	#ifdef CFG_WARN_ONLY
				printf("WARNING: ignored line in config file. %s:%d\n",fn,lineno);
				continue;
	#else
				printf("ERROR parsing config file. %s:%d\n",fn,lineno);
				exit(1);
	#endif
			}
			value=strdup(token);
			if (!parseoption(item,value)) {
	#ifdef CFG_WARN_ONLY
				printf("WARNING: ignored error in config file. %s:%d\n",fn,lineno);
	#else
				printf("ERROR parsing config file. %s:%d\n",fn,lineno);
				exit(1);
	#endif
			}
			free(item); free(value);
	       	}
       	}
	fclose(config_fp);
       	return 0;
}

void xjadeorc (void) {
        char *home;
        char filename[PATH_MAX];
	if ((strlen(SYSCFGDIR) + strlen(XJADEORC) + 2) < PATH_MAX) {
		sprintf(filename, "%s/%s", SYSCFGDIR, XJADEORC);
		if (testfile(filename)) readconfig(filename);
	}
	home = getenv("HOME");
	if ((strlen(home) + strlen(XJADEORC) + 2) < PATH_MAX) {
		sprintf(filename, "%s/.%s", home, XJADEORC);
		if (testfile(filename)) readconfig(filename);
	}
	if ((strlen(home) + strlen(XJADEORC) + 2) < PATH_MAX) {
		sprintf(filename, "./%s", XJADEORC);
		if (testfile(filename)) readconfig(filename);
	}
}
