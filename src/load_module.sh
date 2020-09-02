#! /bin/bash

# THIS FILE MUST BE RUN AS ROOT
if [ -f fbswap.ko ]; then
	insmod fbswap.ko
else
	echo 'fbswap.ko: not found!'
	echo 'Please run `make` as user...'
fi
