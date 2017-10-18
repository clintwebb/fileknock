/* 
   FileKnock Daemon 
   by Clinton Webb (webb.clint@gmail.com)
   October 11th, 2017.  

	This service will use config files located in any of the following locations.
	  /etc/fileknock.d/
	  /opt/fileknock/etc/fileknock.d/
	  /var/fileknock.d/

	It essentially monitors the activity of files or directories using inotify(2), and perform some actions based on that activity.

	For example, you can use it to monitor a directory, and whenever a file is Modified and Closed, perform some action.

*/


#include <assert.h>
#include <dirent.h> 
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "configfile.h"

typedef struct {
	int wd;
	const char *path;
	const char *closedExec;
	const char *closedWriteExec;
} pathwatch_t;


typedef struct {
	
} filewatch_t;


typedef struct {
	
	int infd;	// INOTIFY API File-descriptor.  This is used to access the INOTIFY API.
	
	pathwatch_t **paths;
	int pathcount;
	
	filewatch_t **files;
	int filecount;
	
} maindata_t;




// This static variable will be used to indicate that a signal has been caught that indicates the process should exit.
int keeprunning = 1;

void sig_handler(int sig) {
	if (sig == SIGINT) {
		// Something is indicating that this application should exit.
		// the main blocker in the loop is poll(), which should return on a signal.

		assert(keeprunning == 1);
		keeprunning == 0;
	} 
	else if (sig == SIGHUP) {
		// HUP should be used to trigger a reload of the config (potentially without interrupting service).
		
		// this functionality is incomplete.
		assert(0);
	}
}




// This function will load all config files found in the specified path.   The config details will be examined to determine the actions that need to happen, and are added to a master config structure.
static void process_config_dir(maindata_t *data, const char *configpath)
{
	assert(data);
	assert(configpath);

	// make sure that we already have a reference to the INOTIFY API.
	assert(data->infd >= 0);
	
	size_t pathlen = strlen(configpath);
	assert(pathlen > 0);
	
	// open the directory.
	DIR *d = opendir(configpath);
	if (d) {
		struct dirent *dir;
		
		while ((dir = readdir(d)) != NULL)
		{
			assert(dir);
			assert(dir->d_name);
			
			// if the filename does NOT start with a '.', then we will process it.  Note this means config files can be disabled by putting '.' in front of the filename as per standard practice.
			if (dir->d_name[0] != '.') {
				// build the filepath to the config file.
				char *filepath = malloc(pathlen + 1 + strlen(dir->d_name) + 2);
				sprintf(filepath, "%s/%s", configpath, dir->d_name);
				printf("Config file: %s\n", filepath);
				
				CONFIG config = config_load(filepath);
				if (config) {
					// now that we have found a config file, and loaded it, we need to examine its properties to determine what we need to put in the master config structure.

					const char * pathcheck = config_get(config, "MonitorPath");
					if (pathcheck) {
						// we have found a config file that is monitoring a path.
						printf("Path Monitor: %s\n", pathcheck);

						assert((data->pathcount == 0 && data->paths == NULL) || (data->pathcount > 0 && data->paths));
						data->paths = realloc(data->paths, sizeof(pathwatch_t*)*(data->pathcount + 1));
						assert(data->paths);
						data->paths[data->pathcount] = calloc(1, sizeof(pathwatch_t));
						assert(data->paths[data->pathcount]);
						data->paths[data->pathcount]->wd = -1;
						
						assert(data->paths[data->pathcount]->path == NULL);
						data->paths[data->pathcount]->path = strdup(pathcheck);
						assert(data->paths[data->pathcount]->path);

						assert(data->paths[data->pathcount]->closedExec == NULL);
						assert(data->paths[data->pathcount]->closedWriteExec == NULL);
						
						// We will look at the events the config wants to trigger on, and we will build a mode mask.  
						// After we have checked all the options, we will add it to the  INOTIFY watch list.
						int mode=0;
						
						// now that we know we are watching a path, we need to check for any actions that may be resulting from it.
						const char * closedexec = config_get(config, "FileClosedExec");
						if (closedexec) {
							// there is an action to be performed if the file is closed.
							data->paths[data->pathcount]->closedExec = strdup(closedexec);
							mode |= IN_CLOSE;
						}
						
						const char * closedwriteexec = config_get(config, "FileClosedWriteExec");
						if (closedwriteexec) {
							// there is an action to be performed if the file is closed.
							data->paths[data->pathcount]->closedWriteExec = strdup(closedwriteexec);
							mode |= IN_CLOSE_WRITE;
						}
						
						if (mode != 0) {
							// we have finished checking the different options, now we need to add to the watch descriptors.
							assert(data->paths[data->pathcount]->wd == -1);
							assert(data->infd > 0);
							assert(data->paths[data->pathcount]->path);
							
							data->paths[data->pathcount]->wd = inotify_add_watch(data->infd, data->paths[data->pathcount]->path, mode);

							if (data->paths[data->pathcount]->wd == -1) {
								fprintf(stderr, "Cannot watch '%s'\n", data->paths[data->pathcount]->path);
								assert(0);
							}
						}
						
						data->pathcount ++;
						assert(data->pathcount > 0);
					}
					
					
					config_free(config);
					config = NULL;
				}
				
			}
		}

		closedir(d);
	}
}


int main(void)
{
	// we create a structure that will contain all the major config that we need to use.
	maindata_t *data = calloc(1, sizeof(maindata_t));
	assert(data->pathcount == 0);
	assert(data->paths == NULL);
	assert(data->filecount == 0);
	assert(data->files == NULL);

	// Create interface to the inotify kernel API;
	assert(data->infd == 0);
	data->infd = inotify_init1(IN_NONBLOCK);
	if (data->infd == -1) {
		perror("inotify_init1");
		exit(EXIT_FAILURE);
	}
	assert(data->infd >= 0);
	
	// first we need to look in the directory locations for the config files.
	process_config_dir(data, "/etc/fileknock.d");
	process_config_dir(data, "/opt/fileknock/etc/fileknock.d");
	process_config_dir(data, "/usr/local/etc/fileknock.d");
	process_config_dir(data, "./fileknock.d");

	// setup the signal handler so that we can exit if we are being told to, and potentially to reload the config on HUP.
	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	

	// Now that we have read in all the config, and setup all the watches, we need to poll the interface to know when changes have occurred.	
	assert(data->infd);
	nfds_t nfds = 1;
	struct pollfd fds[nfds];
	fds[0].fd = data->infd;
	fds[0].events = POLLIN;

	while (keeprunning == 1) {
		// poll for API activity. Note that we are using '-1' as a timeout, this will block and not return until there is activity.
		int poll_num = poll(fds, nfds, -1);
		if (poll_num == -1) {
			if (errno != EINTR) {
				fprintf(stderr, "Unexpected error occured while polling for INOTIFY API activity.");
			}
		}
		else {
			// we have some activity.
			assert(poll_num == 1);
			
			if (fds[0].revents & POLLIN) {
				// Inotify events are available
	//			handle_events(fd, wd, argc, argv);
				fprintf(stderr, "Activity detected.\n");
			}
		}
	}

	fprintf(stderr, "Exiting.\n");
	
// 	assert(0);
	

	// We are exiting, there is no reason to bother clearing out objects, structures and file-descriptors, as they will all be free'd by the system when the process exits.
	data = NULL;
		
	exit(EXIT_SUCCESS);
}

