bin_PROGRAMS = serial2serial 
serial2serial_SOURCES = main.c
serial2serial_CFLAGS = -Wall
serial2serial_LDFLAGS = 
serial2serial_LDADD = -lm

serial2serialbindir=$(bindir)

install-exec-hook:
	cp utils/serial2serial.sh $(serial2serialbindir)/
	chmod 777 $(serial2serialbindir)/serial2serial.sh

uinstall-hook:
	rm $(serial2serialbindir)/serial2serial.sh

