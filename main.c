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
char pcap_path[255] = "/tmp/serial.pcap";
uint8_t pcap_debug = 0;
uint32_t tty_speed=57600;

#define MAX_BUF 256
int telem1_fd=0, telem2_fd=0;


void catch_signal(int sig)
{
    printf("signal: %i\n",sig);
    stop = 1;
}

void print_usage() {
    printf("Usage: %s -a [telem1_uart] -b [telem2_uart] -s [speed] -d\n",PACKAGE_NAME);
    printf("-h\thelp\n");
    printf("-d\tactivate pcap capture into %s\n",pcap_path);      
    printf("[telem1_uart] path to telemetry1 uart [defaults: %s]\n",telem1_path);
    printf("[telem2_uart] path to telemetry2 uart [defaults: %s]\n",telem2_path);
    printf("[speed] serial port speed [defaults: %u]\n",tty_speed);
}

int set_defaults(int c, char **a) {
    int option;
    while ((option = getopt(c, a,"a:b:s:d")) != -1) {
        switch (option)  {
            case 'a': strcpy(telem1_path,optarg); break;
            case 'b': strcpy(telem2_path,optarg); break;
	        case 's': tty_speed = atoi(optarg); break;
            case 'd': pcap_debug = 1; break;
            default:
                print_usage();
                return -1;
                break;
        }
    }
    return 0;
} 

int fp1 = 0;

void pcap_header(int fp) {
        uint32_t magic_number=0xa1b2c3d4;
        uint16_t version_major=2;
        uint16_t version_minor=4;  /* minor version number */
        int32_t  thiszone=0;       /* GMT to local correction */
        uint32_t sigfigs=0;        /* accuracy of timestamps */
        uint32_t snaplen=65535;        /* max length of captured packets, in octets */
        //uint32_t network=195;        /* data link type */
        uint32_t network=113;        /* data link type */

        write(fp,&magic_number,4);
        write(fp,&version_major,2);
        write(fp,&version_minor,2);
        write(fp,&thiszone,4);
        write(fp,&sigfigs,4);
        write(fp,&snaplen,4);
        write(fp,&network,4);
}

void pcap_packet(int fp, char *buf, int len, int dir) {
    static uint32_t sec=0;
    static uint32_t usec=0;

    struct timeval tv;
    gettimeofday(&tv,NULL);
    sec = tv.tv_sec;
    usec = tv.tv_usec;
/*
    usec += 100;
    if (usec==1000000) {
        sec++;
        usec=0;
    }
*/

    uint32_t ts_sec = sec;
    uint32_t ts_usec = usec;
    uint32_t incl_len=len+16;
    uint32_t orig_len=len+16; 

    write(fp,&ts_sec,4);
    write(fp,&ts_usec,4);
    write(fp,&incl_len,4);
    write(fp,&orig_len,4);

    //packet header
    uint16_t v;
    v=(dir==0?4:0); //0-to us; 4-from us
    v=htons(v);
    write(fp,&v,2);

    v=0;
    write(fp,&v,2);

    v=0; //address length
    write(fp,&v,2);

    uint64_t v1=0; //address
    write(fp,&v1,8);

    v=0; //1-without an 802.2 LLC header
    v=htons(v);
    //v=ntohs(v);
    write(fp,&v,2);

    write(fp,buf,len);
}

void pcap(int s, char *buf, int len) {
    static int init = 0;
    static int site1 = -1;
    if (site1==-1) site1=s;


    if (init==0) {
        fp1 = open("/tmp/serial.pcap",O_WRONLY | O_CREAT, 0777);
        pcap_header(fp1);
        init++;
    }

    if (s==site1) pcap_packet(fp1,buf,len,0);
    else pcap_packet(fp1,buf,len,1);
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
            
    if (pcap_debug) pcap(s,buf,len);
    //pcap(s,buf,len);

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

    printf("Started. On %s and %s with speed: %u\n",telem1_path,telem2_path,tty_speed);
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
    if (fp1) close(fp1);
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
	default: return 0;
    }
}

int uart_open(const char *path, int flags) {
    int ret = open(path, flags);

    if (ret<0) {
        printf("open failed on %s [%i] [%s]\n",path,errno,strerror(errno));
        return ret;
    }

    if (get_tty_speed(tty_speed)==0) {
	printf("Incorrect serial speed: %u\n",tty_speed);
	return -1;
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

