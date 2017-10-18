ALL: fileknockd

fileknockd: fileknockd.c configfile.o
	gcc -o fileknockd $^

configfile.o: configfile.c configfile.h
	gcc -c -o configfile.o configfile.c
	
install: fileknockd
	cp fileknockd /usr/bin/

configtest: configtest.c configfile.o
	gcc -o configtest configtest.c configfile.o


