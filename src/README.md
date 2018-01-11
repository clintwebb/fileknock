# fileknock
Service that watches files and folders, performing some action when specified activities occur.

This service will use config files in `/etc/fileknock.d/`, `/opt/fileknock/etc/fileknock.d/` or `/var/fileknock.d/` and will execute operations based on the config in those directories.

Note that a lot of the functionality that fileknockd provides, can be done with other tools like lsyncd.  However it should be noted that tools like lsyncd are geared toward a solution that syncs data between systems, however, fileknockd is more about triggering some action from an activity.  It can also be noted that fileknockd can also be configured to replace lsyncd and vice-a-versa.

Example config files:
```
# Monitor a specific path, and when files are Closed in those paths, it will execute a script as a specific user.
MonitorPath=/data/reports
FileClosedExec=/usr/bin/action.sh
RunUser=fxpuser
```

```
# Monitor a specific file:
MonitorFile=/data/error.txt
FileModifiedExec=/usr/bin/error_action.sh
```

When the running script is executed, several environment variables are set to indicate what actually changed 

```
FK_FILE=/data/error.txt
FK_ACTION=CLOSED
```

When the fileknock daemon detects a change that causes a trigger to fire, it is unable to actually ignore the events for that particular file while it is being processed.  Because the trigger will likely cause the action to cause more events while it is doing its action, care should be taken is setting triggers and actions for files.




