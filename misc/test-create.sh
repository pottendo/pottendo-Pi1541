#!/bin/bash
#
# testprogram to create some images, see here: https://github.com/pottendo/pottendo-Pi1541/issues/4
#
piip="192.168.188.31"

while [ ! x"${opts}" = x"break" ] ; do
    case "$1" in
	-pi)
	    shift
	    piip=$1
	    shift
	    ;;
	-d)
	    del=y
	    shift
	    ;;
	*)
	    opts="break"
	    ;;
    esac
done

if [ $# -ne 1 ]; then
    echo "Usage: $0 [-p IP] [-d] <max-number>"
    exit 1
fi

pi="http://${piip}"

max="$1"
i=1
while [ "$i" -le "$max" ]; do
    ti=`printf "a%02d.d64\n" "$i"`
    if [ x${del} = "x" ] ; then 
	echo "creating...${ti}"
        curl -s ${pi}/index.html?%5BDIR%5D\&\&%5BNEWDISK%5D\&${ti} >/dev/null
    else
	echo "deleting...${ti}"
	curl -s ${pi}/index.html?%5BDIR%5D\&\&%5BDEL%5D\&${ti} >/dev/null
    fi
    i=$((i + 1))
done
