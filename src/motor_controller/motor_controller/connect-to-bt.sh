if [ ! -e /dev/rfcomm0 ]; then
    sudo rfcomm bind rfcomm0 00:21:13:00:0C:E3 1
fi
cat /dev/rfcomm0