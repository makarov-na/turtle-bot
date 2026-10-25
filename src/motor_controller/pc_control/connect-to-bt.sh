#!/usr/bin/env bash

if [ ! -e /dev/rfcomm0 ]; then
    sudo rfcomm bind rfcomm0 00:21:13:00:0C:E3 1
fi
cat /dev/rfcomm0

# Если stty ругается No route to host:
# sudo rfcomm release 0 && sudo rfcomm bind rfcomm0 00:21:13:00:0C:E3 1
# и снова bluetoothctl connect.
