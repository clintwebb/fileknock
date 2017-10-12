#include <stdio.h>
#include <assert.h>

#include "configfile.h"

void main(void)
{
	CONFIG config;
	
	
	config = config_load("test.conf");
	if (config) {
		printf("Config loaded.\n");
		const char *writefile = config_get(config, "WriteFile");
		if (writefile) { printf("Write File: '%s'\n", writefile);}
		
		int active = config_get_bool(config, "Activate");
		if (active != 0) {printf("Activate!\n");}
		else { printf("Not Active!\n");}

		// we are finished with the config file, so we can delete it.
		config_free(config);
		
	}
	else {
		printf("Config FAILED.\n");
	}
}
