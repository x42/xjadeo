#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define MAX_LINE_LEN 256 
#define PATH_MAX 255

#define SYSCFGDIR "/etc"
#define XJADEORC "xjadeorc"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <xjadeo.h>

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
	} else if (!strncasecmp(item,"VERBOSE",7)) {
		if (!strncasecmp(value,"yes",3)){
			want_verbose=1; rv=1;
		}
	}
//#ifdef HAVE_FT
	else if (!strncasecmp(item,"FONTFILE",8)) {
		strncpy(OSD_fontfile,value,1023);
		OSD_fontfile[1023]=0; // just to be sure.
	}
//#endif
	return (rv);
}


int readconfig (char *fn) {
       	FILE* config_fp;
       	char line[MAX_LINE_LEN];
       	char* token, *item,*value;
	
       	if (!(config_fp = fopen(fn, "r"))) {
		fprintf(stderr,"configfile failed: %s (%s)\n",fn,strerror(errno));
		return (-1);
	}
#if 0
	fprintf(stdout,"INFO: parsing configfile: %s\n",fn);
#endif
       	while( fgets( line, MAX_LINE_LEN-1, config_fp ) != NULL ) {
		line[MAX_LINE_LEN-1]=0;
	       	token = strtok( line, "\t =\n\r" ) ; 
		if( token != NULL && token[0] != '#' && token[0] != ';') {
			item=strdup(token);
			token = strtok( NULL, "\t =\n\r" ) ; 
			value=strdup(token);
			parseoption(item,value);
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
