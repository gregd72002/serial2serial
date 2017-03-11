#!/bin/sh
while [ 1 ]; do
    sleep 1;
    if [ -c $1 ] && [ -c $2 ] && [ $# -eq 2 ]; then 
	/usr/local/bin/serial2serial -a $1 -b $2
    elif [ -c $1 ] && [ -c $2 ] && [ $# -eq 3 ]; then 
	/usr/local/bin/serial2serial -a $1 -b $2 -s $3
    else
	echo "Incorrect parameters";
	exit 1;
    fi
done

