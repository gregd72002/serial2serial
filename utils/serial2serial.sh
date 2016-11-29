#!/bin/sh
while [ 1 ]; do
    sleep 1;
    if [ -c $1 ] && [ -c $2 ]; then 
	/usr/local/bin/serial2serial -a $1 -b $2
    fi
done

