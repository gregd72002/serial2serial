#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sched.h>
#include <getopt.h>
#include <termios.h>

uint8_t stop = 0;

char telem1_path[255] = "/dev/rfcomm0";
char telem2_path[255] = "/dev/ttyAMA0";

uint32_t tty_speed=57600;

#define MAX_BUF 256
int telem1_fd=0, telem2_fd=0;


void catch_signal(int sig)
{
    printf("signal: %i\n",sig);
    stop = 1;
}

void print_usage() {
    printf("Usage: %s -a [telem1_uart] -b [telem2_uart] -s [speed]\n",PACKAGE_NAME);
    printf("-h\thelp\n");;        
    printf("[telem1_uart] path to telemetry1 uart [defaults: %s]\n",telem1_path);
    printf("[telem2_uart] path to telemetry2 uart [defaults: %s]\n",telem2_path);
    printf("[speed] serial port speed [defaults: %u]\n",tty_speed);
}

int set_defaults(int c, char **a) {
    int option;
    while ((option = getopt(c, a,"a:b:s:")) != -1) {
        switch (option)  {
            case 'a': strcpy(telem1_path,optarg); break;
            case 'b': strcpy(telem2_path,optarg); break;
	    case 's': tty_speed = atoi(optarg); break;
            default:
                print_usage();
                return -1;
                break;
        }
    }
    return 0;
} 

void forward_telem(int s, int t) {
    int len,len1;
    char buf[MAX_BUF];

    len = read(s,buf,MAX_BUF);
    
    if (len<=0) {
        printf("reading error [%i] [%s]\n",errno,strerror(errno));
        stop = 1;
        return;
    }
            
    len1 = write(t,buf,len);
            
    if (len!=len1) {
        printf("writing error [%i] [%s]\n",errno,strerror(errno));
        stop = 1;
        return;
    }           
}

void loop() {
    fd_set fdlist,rlist;
    struct timeval tv;
    int ret;

    FD_ZERO(&fdlist);

    FD_SET(telem1_fd, &fdlist);
    FD_SET(telem2_fd, &fdlist);

    printf("Started.\n");
    while (!stop) {
        rlist = fdlist;
        tv.tv_sec = 1; 
        tv.tv_usec = 0;

        ret = select (FD_SETSIZE, &rlist, NULL, NULL, &tv);

        if (ret < 0) {
              perror ("select");
              stop = 1;
              return;
        } 

        if (ret == 0) { //timeout
            continue; 
        }

        if (FD_ISSET(telem1_fd,&rlist)) {
            forward_telem(telem1_fd,telem2_fd);
        }

        if (FD_ISSET(telem2_fd,&rlist)) {
            forward_telem(telem2_fd,telem1_fd);
        }
    }  
}

void cleanup() {
    if (telem1_fd) close(telem1_fd);
    if (telem2_fd) close(telem2_fd);
    printf("Bye.\n");
}

speed_t get_tty_speed(uint32_t v) {
    switch(v) {
	case 9600: return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
	case 57600: return B57600;
	case 115200: return B115200;
	default: return B0;
    }
}

int uart_open(const char *path, int flags) {
    int ret = open(path, flags);

    if (ret<0) {
        printf("open failed on %s [%i] [%s]\n",path,errno,strerror(errno));
        return ret;
    }

    struct termios options;
    tcgetattr(ret, &options);
    
    options.c_cflag &= ~(CSIZE | PARENB);
    options.c_cflag |= CS8;

    
    options.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
                     INLCR | PARMRK | INPCK | ISTRIP | IXON);
    
    options.c_oflag = 0;

    
    options.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

    if(cfsetispeed(&options, get_tty_speed(tty_speed)) < 0 || cfsetospeed(&options,
	get_tty_speed(tty_speed)) < 0) {
        return -1;
    }

    tcflush(ret, TCIFLUSH);
    tcsetattr(ret, TCSANOW, &options);
    return ret; 
}

int main(int argc, char **argv) {

    signal(SIGTERM, catch_signal);
    signal(SIGINT, catch_signal);

    if (set_defaults(argc,argv)) return -1;

    telem1_fd = uart_open(telem1_path, O_RDWR | O_NOCTTY | O_SYNC | O_NDELAY | O_NONBLOCK);
    if (telem1_fd<0) {
        cleanup();
        return -1;
    }

    telem2_fd = uart_open(telem2_path, O_RDWR | O_NOCTTY | O_SYNC | O_NDELAY | O_NONBLOCK);
    if (telem2_fd<0) {
        cleanup();
        return -1;
    }

    loop();

    cleanup();

    return 0;
}

