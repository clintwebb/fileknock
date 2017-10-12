// config.h

/* 
 * Written by Clinton Webb
 * Published under the GNU Lesser Licence.  See configfile.LICENSE.
 * 
 * This is a generic config file loader.   No application specific code should be here.
 * Since this initial implementation will only use a single config file, it will be done from that perspective.  
 * Multiple config objects can be created to open multiple config files. 
*/

#ifndef __CONFIGFILE_H
#define __CONFIGFILE_H

// The returning config object will be type de-referenced outside of the library (it will simply be a pointer to a void object).
// The structure of it really doesn't need to be exposed externally.
typedef void * CONFIG;

// load the configfile
CONFIG config_load(const char *path);

const char * config_get(CONFIG config, const char *key);
int config_get_bool(CONFIG config, const char *key);
long long config_get_long(CONFIG config, const char *key);

void config_free(CONFIG config);



#endif