#!/bin/bash

pkill -f ./server
make 
make clean
./server -port $1 -root /Users/liqi17thu/Documents/GitHub/FTP_server
