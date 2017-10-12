// configfile.c

/* 
 * Written by Clinton Webb
 * Published under the GNU Lesser Licence.  See configfile.LICENSE.
 * 
 * This is a generic config file loader.   No application specific code should be here.
 * Since this initial implementation will only use a single config file, it will be done from that perspective.  
 * Multiple config objects can be created to open multiple config files. 
*/


#include "configfile.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>



typedef struct {
	char *key;
	char *value;
} config_pair_t;


typedef struct {
	int items;
	config_pair_t *pairs;
	char *path;
} config_t;




// free the resources for the current config that is loaded.
void config_free(CONFIG configptr)
{
	config_t *config = configptr;
	assert(config);
	
	if (config->pairs) {
		assert(config->items >= 0);
		
		while(config->items > 0) {
			config->items --;
			assert(config->pairs[config->items].key);
			free(config->pairs[config->items].key);
			config->pairs[config->items].key = NULL;
			
			assert(config->pairs[config->items].value);
			free(config->pairs[config->items].value);
			config->pairs[config->items].value = NULL;
		}
		
		free(config->pairs);
	}
	
	if (config->path) { free(config->path); }
	
	free(config);
	config = NULL;
}


// load a config file into a structure.   Return a pointer to a config object on success, otherwise return NULL.
CONFIG config_load(const char *path)
{
	assert(path);
	config_t *config = NULL;
	
	// open the file
	int fd = open(path, 0);
	if (fd < 0) {
		#ifndef NDEBUG
			printf("an error occurred trying to open the file: %s\n", path);
		#endif
		assert(config == NULL);
	}
	else {
		// since we have successfully opened the file, we can create the config object and then try to parse it.
		config = calloc(1, sizeof(config_t));
		assert(config);
		assert(config->items == 0);
		assert(config->pairs == NULL);
	
		// store the path to the config, so that we can know which one it is if we switch configs.
		config->path = strdup(path);

		// find out the length of the file.
		struct stat sb;
		fstat(fd, &sb);
		off_t file_length = sb.st_size;
		
		// read the entire file into a buffer (using mmap).
		char *file_buffer = mmap(NULL, file_length, PROT_READ, MAP_PRIVATE, fd, 0);
		if (file_buffer != MAP_FAILED) {
			
// 			// copy the data from the file to a new string, because it will get modified while we are parsing it.
			assert(file_buffer);
			char *buffer = strndup(file_buffer, file_length);
			assert(buffer);
			
			// first parse, remove all comments and blank lines.
			char *next_line = buffer;
			while (next_line) {
				char *line = strsep(&next_line, "\n");
				int line_length = strlen(line);
				
				if (line_length > 0) {
					// trim all whitespace from the beginning of the line.
// 					printf("raw line: [%s]\n", line);
					while(line_length > 0 && (line[0] == ' ' || line[0] == '\t' || line[0] == '\r')) { 
						line ++; 
						line_length --;
					}
					
					if (line_length > 0) {
// 						printf("after trim front: [%s]\n", line);
						
						if (line[0] != '#' ) {
						
							// trim all whitespace at the end of the line
							assert(line_length > 0);
							line_length--;
							while(line_length > 0 && (line[line_length] == ' ' || line[line_length] == '\t' || line[0] == '\r')) {
								line[line_length] = 0;
								line_length --;
							}
// 							printf("after trim end: [%s]\n", line);
							
							// we now have a line that is trimmed.  
							// second parse, split on '='
							char *value=index(line, '=');
							if (value) {
								value[0] = 0;
								value++;
								
								// trim leading spaces from values.
								while(value[0] == ' ' || value[0] == '\t' || value[0] == '\r') {
									value ++;
								}
								
								// trim trailing spaces from values.
								int len = strlen(value) - 1;
								while(len > 0 && (value[len] == ' ' || value[len] == '\t')) {
									value[len] = 0;
									len --;
								}
								
								// make space in the array and add the new pair to it.
// 								printf("%s='%s'\n", line, value);
								assert(config);
								assert(config->items >= 0);
								config->pairs = realloc(config->pairs, sizeof(config_pair_t) * (config->items+1));
								assert(config->pairs);
								
								config->pairs[config->items].key = strdup(line);
								config->pairs[config->items].value = strdup(value);
								config->items++;
							}
						}
					}
				}
			}
			
			// we have finished parsing the buffer, we can free it now.
			free(buffer);
			buffer =  NULL;
		}
		
		// close the file
		close(fd);
		fd = -1;
	}
	
	// if we were able to open the file, then we return a config object.
	return((CONFIG) config);
}


// Get a config value based on the key.  Will return a string.
const char * config_get(CONFIG configptr, const char *key)
{
	config_t *config = configptr;
	assert(config);
	
	assert(key);
	const char *value = NULL;

	int i;
	for (i=0; i<config->items; i++) {
		assert(config->pairs[i].key);
		assert(config->pairs[i].value);
		
// 		printf("checking: %s\n", config->pairs[i].key);
		
		if(strcasecmp(key, config->pairs[i].key) == 0) {
			value = config->pairs[i].value;
			i = config->items;
		}
	}
	
	return(value);
}


// get the config value and convert to a long.  If the value does not exist, or does not convert, 
// then a 0 is returned.
long long config_get_long(CONFIG configptr, const char *key)
{
	config_t *config = configptr;
	assert(config);
	
	long long result = 0;
	
	assert(key);
	const char *value = config_get(configptr, key);
	if (value) {
		result = atoll(value);
	}
	return(result);
}


// This will search for the key, and if it is 'yes' or 'true' or 1, then it will return a non-zero 
// number (most probably 1).  Otherwise, it will return 0.
int config_get_bool(CONFIG configptr, const char *key)
{
	int result = 0;
	config_t *config = configptr;
	assert(config);
	
	assert(key);
	const char *value = config_get(configptr, key);
	if (value) {
		const char *ptr = value;
		
		// skip any wrapping chars that are likely to be used.
		while(*ptr == '"' || *ptr == '\'' || *ptr == '(' || *ptr == '[') {
			ptr ++;
		}
		
		if (*ptr == 't' || *ptr == 'T' || *ptr == 'y' || *ptr == 'Y' || *ptr == '1') {
			result = 1;
		}
	}
	
	return(result);
}


// fin - configfile.c