#!/bin/bash
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# written by @snaff,F64
#
# To run this script, several developer tools must be present
#   cross compiler - refer to readme
#   make, wget, unzip, git, sed, patch
#
# this doesn't do many sanity checks, so check carefully
# used on Arch Linux, x86 host system

# Generate your index file using:
# find /mnt/tmp/1541/ -type f |egrep -v '(.sid|.txt)$' | sed 's_/mnt/tmp/1541/__' > 1541.txt
index="1541.txt"
pi="http://pi1541.lan"
search=$1
num=$2

if ! jq --help > /dev/null ; then 
  echo "need \"jq\" - giving up."
  exit 1
fi

while [ ! x"${opts}" = x"break" ] ; do
    case "$1" in 
    -i)
	    shift
      index="$1"
      shift
      ;;
	  -pi) 
      shift
	    pi="$1"
	    shift
	    ;;
    -u)
      shift
      src="$1"
      break
      ;;
  	-h)
	    echo "Usage: mount from index: $0 [-i <index-file>] [-pi http://my.pi.local.address ] search [num]"
      echo "Defaults: -i 1541.txt -pi http://pi1541.lan"
      echo "Usage: upload files/directories: $0 [-pi http://my.pi.local.address ] [-u] file [files...]"
      echo "uploads (-u) are pushed to /1541"
	    shift
	    exit 0
	    ;;
    *)
      opts="break"
      ;;
    esac
done

if [ ! -z "$src" ] ; then
  echo "$@"
  for f in "$@" ; do 
    echo $f
    if [ -d "$f" ] ; then
      echo "uploading directory $f..."
    else
      echo "uploading file $f"
      uimage=$(printf %s "$f" | jq -sRr @uri)
      echo $uimage
      echo curl POST -F \"diskimage=@${f}\" ${pi}
    fi
  done
  exit 0
fi

echo
echo Matching images:
echo
grep -i "$search" $index | sort | grep -n .

if [ -z "$num" ]
then
    echo
    read -p "Disk: " num
fi
image=$(grep -i "$search" $index | sort | head -n $num |tail -n 1)
uimage=$(printf %s "$image" | jq -sRr @uri)

echo
echo mounting $image
echo curl -s $pi/mount-imgs.html?%5BMOUNT%5D\&SD%3A%2F1541%2F"$uimage"
curl -s $pi/mount-imgs.html?%5BMOUNT%5D\&SD%3A%2F1541%2F"$uimage" >/dev/null
