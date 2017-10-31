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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "configfile.h"

typedef struct {
	int wd;
	const char *path;
	const char *file;
	const char *closedExec;
	const char *closedWriteExec;
} watch_t;




typedef struct {
	
	int infd;	// INOTIFY API File-descriptor.  This is used to access the INOTIFY API.
	
	watch_t **watches;
	int watchcount;
} maindata_t;




static watch_t * new_watch(maindata_t *data)
{
	watch_t *watch = NULL;
	
	assert((data->watchcount == 0 && data->watches == NULL) || (data->watchcount > 0 && data->watches));
	data->watches = realloc(data->watches, sizeof(watch_t*)*(data->watchcount + 1));
	assert(data->watches);
	watch = calloc(1, sizeof(watch_t));
	assert(watch);

	assert(watch->path == NULL);
	assert(watch->file == NULL);
	
	watch->wd = -1;
	
	data->watches[data->watchcount] = watch;
	assert(data->watches[data->watchcount]);

	data->watchcount ++;
	assert(data->watchcount > 0);

	return(watch);
}


static void add_watch(maindata_t *data, watch_t *watch, CONFIG config)
{
	assert(data);
	assert(watch);
	
	assert(watch->closedExec == NULL);
	assert(watch->closedWriteExec == NULL);
						
	// We will look at the events the config wants to trigger on, and we will build a mode mask.  
	// After we have checked all the options, we will add it to the  INOTIFY watch list.
	int mode=0;
	
	// now that we know we are watching a path, we need to check for any actions that may be resulting from it.
	const char * closedexec = config_get(config, "FileClosedExec");
	if (closedexec) {
		// there is an action to be performed if the file is closed.
		watch->closedExec = strdup(closedexec);
		mode |= IN_CLOSE;
	}
	
	const char * closedwriteexec = config_get(config, "FileClosedWriteExec");
	if (closedwriteexec) {
		// there is an action to be performed if the file is closed.
		watch->closedWriteExec = strdup(closedwriteexec);
		mode |= IN_CLOSE_WRITE;
	}
	
	if (mode != 0) {
		// we have finished checking the different options, now we need to add to the watch descriptors.
		assert(watch->wd == -1);
		assert(data->infd > 0);
		
		assert(watch->path || watch->file);
		
		if (watch->path) {
			assert(watch->file == NULL);
			watch->wd = inotify_add_watch(data->infd, watch->path, mode);
		}
		else if (watch->file) {
			assert(watch->path == NULL);
			watch->wd = inotify_add_watch(data->infd, watch->file, mode);			
		}
		else {
			// should have been at least path or file watch
			assert(0);
		}


		if (watch->wd == -1) {
			int e = errno;
			if (e == ENOENT) {
				fprintf(stderr, "Cannot watch '%s', %s\n", watch->path ? watch->path : watch->file, strerror(e));
			}
			else {
				perror("Unexpected failure");
			}
			
			// since we couldn't watch it, we should remove this entry.
		}
	}
	
	if (watch->wd == -1) {
		// the watch was not set, we should remove this entry.
		assert(0);
	}
	
	assert(data->watchcount > 0);

}


// This function will load all config files found in the specified path.
// The config details will be examined to determine the actions that need to happen, and are added to a master config structure.
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
			
			// if the filename does NOT start with a '.', then we will process it.
			// Note this means config files can be disabled by putting '.' in front of the filename as per standard practice.
			if (dir->d_name[0] != '.') {
				// build the filepath to the config file.
				char *filepath = malloc(pathlen + 1 + strlen(dir->d_name) + 2);
				assert(filepath);
				sprintf(filepath, "%s/%s", configpath, dir->d_name);
				assert(filepath);
				assert(strlen(filepath) > 0);
				printf("Config file: %s\n", filepath);
				
				CONFIG config = config_load(filepath);
				if (config) {
					// now that we have found a config file, and loaded it, we need to examine its properties to determine what we need to put in the master config structure.

					const char * pathcheck = config_get(config, "MonitorPath");
					if (pathcheck) {
						// we have found a config file that is monitoring a path.
						printf("Path Monitor: %s\n", pathcheck);

						watch_t *watch = new_watch(data);
						assert(watch);
						
						assert(watch->path == NULL);
						watch->path = strdup(pathcheck);
						assert(watch->path);

						add_watch(data, watch, config);
					}

					const char * filecheck = config_get(config, "MonitorFile");
					if (filecheck) {
						// we have found a config file that is monitoring a path.
						printf("File Monitor: %s\n", filecheck);

						watch_t *watch = new_watch(data);
						assert(watch);
						
						assert(watch->file == NULL);
						watch->file = strdup(filecheck);
						assert(watch->file);

						add_watch(data, watch, config);
					}
					
					config_free(config);
					config = NULL;
				}
				
			}
		}

		closedir(d);
	}
}



static char ** add_envp(char * const *envp, int *envpcount, const char * fmt, ...)
{
	int size = 0;
	va_list ap;

	assert(envpcount);
	assert((envp == NULL && *envpcount == 0) || (envp && *envpcount > 0));
	assert(fmt);
	
	// Determine required size 
	va_start(ap, fmt);
	size = vsnprintf(NULL, size, fmt, ap);
	va_end(ap);

	// if there is an error in formatting, then return without adding anything.
	if (size < 0) { return envp;}

	// For the trailing NULL to terminate the string.
	size++;
	
	// allocate the memory
	assert(size > 0);
	char *p = malloc(size);
	assert(p);

	va_start(ap, fmt);
	size = vsnprintf(p, size, fmt, ap);
	assert(size >= 0);
	va_end(ap);

	// now we need to add the new string to the envp array.  Note that the last element in the array needs to be NULL.
	char ** newenv = realloc(envp, (((*envpcount) + 2) * (sizeof(char *))));
	assert(newenv);
	
	newenv[(*envpcount)] = p;
	(*envpcount) ++;
	newenv[(*envpcount)] = (char *) NULL;
	
	return newenv;
}




// Read all available inotify events and process them.
static void handle_events(maindata_t *data)
{
	/* NOTE: Some systems cannot read integer variables if they are not
		properly aligned. On other systems, incorrect alignment may
		decrease performance. Hence, the buffer used for reading from
		the inotify file descriptor should have the same alignment as
		struct inotify_event. */

	char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event *event;

	assert(data);
	
	// Loop while events can be read from inotify file descriptor.
	// note, code inside will force a break from the loop when there is nothing more to process.
	for (;;) {

		// Read some events.
		assert(data->infd);
		ssize_t len = read(data->infd, buf, sizeof(buf));
		if (len == -1 && errno != EAGAIN) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		// If the nonblocking read() found no events to read, then it returns -1 with errno set to EAGAIN. In that case, we exit the loop.
		if (len <= 0) {
			assert(errno == EAGAIN);
			break;
		}

		// Loop over all events in the buffer
		char *ptr;
		for (ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {

			event = (const struct inotify_event *) ptr;
			assert(event);
			
			assert(event->wd >= 0);

			if (data->watches) {
				assert(data->watchcount > 0);
	
				// iterate through the list of watches, to find any watch-descriptors that match.
				int i;
				for (i=0; i < data->watchcount; i++) {
					assert(data->watches[i]);
					watch_t *watch = data->watches[i];
					assert(watch);
					
					assert(watch->wd >= 0);
					assert(watch->path || watch->file);
					
					if (watch->wd == event->wd) {
						// we found a match.  We dont stop here, because it is possible for multiple matches in the list of watches.
						
// 			if (event->mask & IN_OPEN)			printf("IN_OPEN: ");
// 			if (event->mask & IN_CLOSE_NOWRITE)	printf("IN_CLOSE_NOWRITE: ");
// 			if (event->mask & IN_CLOSE_WRITE)	printf("IN_CLOSE_WRITE: ");
		
						if (((event->mask & IN_CLOSE_WRITE) || (event->mask & IN_CLOSE_NOWRITE)) && watch->closedExec) {
							// action is triggered whenever a file is closed for either reading or writing.
							
							pid_t pid = fork();
							if (pid == 0) {
								
								char ** envp = NULL;
								int envp_count = 0;
								
								if (watch->path) {
									assert(watch->path);
									assert(watch->file == NULL);
// 									fprintf(stderr, "Adding ENV: FK_PATH=%s\n", watch->path);
									envp = add_envp(envp, &envp_count, "FK_PATH=%s", watch->path);
									assert(envp_count > 0);
									assert(envp);
									assert(event->len > 0);
									assert(event->name);
// 									fprintf(stderr, "Adding ENV: FK_FILE=%s\n", event->name);
									envp = add_envp(envp, &envp_count, "FK_FILE=%s", event->name);
									assert(envp_count > 1);
									assert(envp);
								}
								else if (watch->file) {
									assert(watch->file);
									assert(watch->path == NULL);
// 									fprintf(stderr, "Adding ENV: FK_PATH=%s\n", watch->file);
									envp = add_envp(envp, &envp_count, "FK_PATH=%s", watch->file);
									assert(envp_count > 0);
									assert(envp);
									assert(event->len > 0);
									assert(event->name);
// 									fprintf(stderr, "Adding ENV: FK_FILE=%s\n", event->name);
									envp = add_envp(envp, &envp_count, "FK_FILE=%s", event->name);
									assert(envp_count > 1);
									assert(envp);
								}
								else {
									// should have been one or the other.
									assert(0);
								}

// 								int result = execle("/bin/sh", "sh", "-c", watch->closedExec , (char *) NULL, (char * const *) envp );
								int result = execve(watch->closedExec , (char *) NULL, (char * const *) envp );

								// if successful, the forked process will be replaced by the functionality specified above.
								
							}
							else if (pid < 0) {
								// An error happened when the fork was attempted.
								assert(0);
							}
							else {
								// This is the parent process.
								printf("Action event triggered.  PID=%d, Action='%s'\n", pid, watch->closedExec);
							}
							
							
						}
						
						if ((event->mask & IN_CLOSE_WRITE) && watch->closedWriteExec) {
							// action is triggered whenever a file is closed for writing.
// 							pid_t pid = spawn_action(event, watch->closedExec);
// 							printf("Action event triggered.  PID=%d, Action='%s'\n", pid, watch->closedExec)
						}
						
						if (watch->path) { 
							printf("%s/", watch->path);
						}
						else {
							assert(watch->file);
							printf("%s", watch->file);
						}
//						assert(event->mask & IN_ISDIR);
						
						if (event->len)
							printf("%s\n", event->name);
						else 
							printf("\n");
						
					}
				}
			}
		}
	}
}



int main(void)
{
	// we create a structure that will contain all the major config that we need to use.
	maindata_t *data = calloc(1, sizeof(maindata_t));
	assert(data->watchcount == 0);
	assert(data->watches == NULL);

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


	// Now that we have read in all the config, and setup all the watches, we need to poll the interface to know when changes have occurred.	
	assert(data->infd);
	nfds_t nfds = 1;
	struct pollfd fds[nfds];
	fds[0].fd = data->infd;
	fds[0].events = POLLIN;

	int keeprunning = 1;
	while (keeprunning == 1) {
		// poll for API activity. Note that we are using '-1' as a timeout, this will block and not return until there is activity.
		int poll_num = poll(fds, nfds, -1);
		if (poll_num == -1) {
			if (errno == EINTR) {
				keeprunning = 0;
			}
			else {
				fprintf(stderr, "Unexpected error occured while polling for INOTIFY API activity.");
			}
		}
		else {
			// we have some activity.
			assert(poll_num == 1);
			
			if (fds[0].revents & POLLIN) {
				// Inotify events are available
				assert(data);
				handle_events(data);
			}
		}
	}

	fprintf(stderr, "Exiting.\n");

	// We are exiting, there is no reason to bother clearing out objects, structures and file-descriptors, as they will all be free'd by the system when the process exits.
	data = NULL;
		
	exit(EXIT_SUCCESS);
}

