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
# or get it from pi1541 using option -g
index="1541.txt"
pi="${PI1541}"

if ! jq --help > /dev/null ; then 
  echo "need \"jq\" - giving up."
  exit 1
fi
#set -x 

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
      upload=1
      shift
      ;;
    -d) 
      shift
      dst="$1"
      shift
      ;;
    -g)
      getindex=1
      shift
      ;;

  	-h)
	    echo "Usage: mount from index: $0 [-i <index-file>] [-pi http://my.pi.local.address ] search [num]"
      echo "Defaults: -i 1541.txt -pi http://pi1541.lan"
      echo "Usage: generate image from running Pi1541: $0 -g [-i <index-file> ]"
      echo "writes a file ${index} in the current host directory, using filename specified by (-i)"
      echo "Usage: upload files/directories: $0 [-pi http://my.pi1541.local.address ] [-d dest-dir] -u dir|file [dir|file ...]"
      echo "uploads (-u) are pushed to /1541 or below if specified with (-d)"
      echo "Use export PI1541=http://my.pi1541.local.address to avoid option [pi]"
	    shift
	    exit 0
	    ;;
    *)
      opts="break"
      search=$1
      num=$2
      ;;
    esac
done

if [ -z ${pi} ] ; then
  pi="http://pi1541.lan"
fi

echo "Using ${pi}..."
export dst pi

upload_file() {
  file=$1
  if [ -d "${file}" ] ; then
    echo creating directory ${dst}/${file}
    _d=$(printf %s "${dst}" | jq -sRr @uri)
    _f=$(printf %s "${file}" | jq -sRr @uri)
    curl -s ${pi}/index.html?%5BDIR%5D\&/${_d}\&%5BMKDIR%5D\&${_f}\& > /dev/null
  else
    dir=`dirname "${file}"`
    fn=`basename "${file}"`
    _d=$(printf %s "${dst}/${dir}" | jq -sRr @uri)
    _d="?%5BDIR%5D\&/${_d}"
    echo uploading ${fn} to ${dst}/${dir}
    curl -s -F "diskimage=@${file}" ${pi}/index.html${_d} >/dev/null
  fi
}

export -f upload_file

if [[ ! (-z "$@") && (${upload} == 1) ]] ; then
  for f in "$@" ; do 
    if [ -d "$f" ] ; then
      echo "uploading directory ${f}..."
      find ${f} -name "*" -exec /bin/bash -c 'upload_file "$0"' {} ${dst} \;
    else
      echo "uploading file $f..."
      if [ ! -z ${dst} ] ; then
        targetdir=$(printf %s "${dst}" | jq -sRr @uri)
        targetdir="?%5BDIR%5D\&/${targetdir}"
        echo "... to ${dst}"
      fi
      curl -s -F "diskimage=@${f}" ${pi}/index.html${targetdir} >/dev/null
    fi
  done
  exit 0
fi

# generate index
if [ ${getindex} == 1 ] ; then
  echo "generating index ${index}"
  curl -s ${pi}/getindex.html >${index}
fi

echo searching for ${search}
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
echo curl -s $pi/mount-imgs.html?%5BMOUNT%5D\&2F"$uimage"
curl -s $pi/mount-imgs.html?%5BMOUNT%5D\&%2F"$uimage" >/dev/null
