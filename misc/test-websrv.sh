#!/bin/bash
#
# testprogram to test memory profile when accessing the webserver
#
piip="192.168.188.31"

while [ ! x"${opts}" = x"break" ] ; do
    case "$1" in
	-pi)
	    shift
	    piip=$1
	    shift
	    ;;
	*)
	    opts="break"
	    ;;
    esac
done

if [ $# -ne 1 ]; then
    echo "Usage: $0 [-p IP] <max-number>"
    exit 1
fi

pi="http://${piip}"

cmds="/indext.html /mount-imgs.hml /edit-config.html /update.html /logger.html"

max="$1"
i=1
while [ "$i" -le "$max" ]; do
	for c in ${cmds} ; do 
		curl -s ${pi}${c} | grep -Po 'pottendo-Pi1541.*?Memory: \K\d+kB/\d+kB'
		if [ ${PIPESTATUS[0]} -ne 0 ] ; then
			echo "curl failed, waiting 30s..."
			sleep 30
		fi
	done
    i=$((i + 1))
done
