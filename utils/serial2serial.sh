#!/bin/sh
while [ 1 ]; do
    sleep 1;
    if [ -c $1 ] && [ -c $2 ]; then 
	stdbuf -oL /usr/local/bin/serial2serial -a $1 -b $2 >> /tmp/serial2serial.log 2>&1
    fi
done

