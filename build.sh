#!/bin/sh
# 
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
# written by pottendo
#
# To run this script, several developer tools must be present
#   cross compiler - refer to readme
#   make, wget, unzip, git, sed, patch
#
# this doesn't do many sanity checks, so check carefully
# used on Arch Linux, x86 host system

base=`pwd`

# some sanity checks
if git remote get-url origin | grep `basename ${base}` ; then
    echo "Running in ${base}, OK!"
else
    echo "$base should be the checkout pottendo-Pi1541"
    exit 1
fi

arm-none-eabi-gcc --version 2>&1 > /dev/null
if [ ! $? -eq 0 ] ; then
    echo "missing 32 bit compiler - refer to Readme"
    exit 1
fi
aarch64-none-elf-gcc --version 2>&1 > /dev/null
if [ ! $? -eq 0 ] ; then
    echo "missing 64 bit compiler - refer to Readme"
    exit 1
fi

tag="none"
archs="pi3-32 pi3-64 pi4-32 pi4-64"
while [ ! x"${opts}" = x"break" ] ; do
    case "$1" in 
    -t)
	    shift
        tag="$1"
        shift
        ;;
	-a) shift
	    archs="$1"
	    shift
	    ;;
	-c)
	    checkout="yes"
	    shift
	    ;;
	-h)
	    echo "Usage: $0 [-t <release-tag>] [-a pi3-32|pi3-64|pi4-32|pi4-64|pi5-64] [-c checkout circle-stdlib]" 
	    shift
	    exit 0
	    ;;
    *)
        opts="break"
        ;;
    esac
done

CIRCLE=${base}/../circle-stdlib
PPI1541=${base}
ROMS="dos1581-318045-02.bin dos1541ii-251968-03.bin dos1541-325302-01%2B901229-05.bin"

# if a tag is given, it's pottendos release build, user shouldn't bother
if [ x${tag} != "xnone" ] ; then
    cd ${PPI1541}
    git pull
    if git tag -l |grep ${tag} >/dev/null ; then
	    echo "Tag already set: ${tag}"
    else
	    echo "No tag set, refusing"
	    exit 1
    fi
    RELEASE=${base}/../${tag}
    mkdir ${RELEASE} 2>/dev/null
    # Check if legacy build still works
    cd ${PPI1541}
    if make RASPPI=0 legacy > make-PiZero.log; then
	    echo "successully built for PiZero, legacy code base"
	    cp kernel.img ${RELEASE}
    else
	    echo "failed to build legacy codebase"
	    exit 1
    fi
else
    # install in builddir, where the checkouts have been done
    RELEASE=${base}/..
fi

if [ x${checkout} = "xyes" ] ; then
    cd ${base}/..
    rm -rf ${CIRCLE}
    git clone --recursive https://github.com/smuehlst/circle-stdlib.git
    cd ${CIRCLE}/libs/circle
    patch -p1 < ../../../pottendo-Pi1541/src/Circle/patch-circle-V49.0.diff
    mkdir ${RELEASE}/Pi-Bootpart 2>/dev/null
    # fetch bootfiles for RPis
    cd ${CIRCLE}/libs/circle/boot
    make
    cp bootcode.bin fixup* start* bcm2711-rpi-4-b.dtb ${RELEASE}/Pi-Bootpart
    # fetch WiFi firmware
    cd ${CIRCLE}/libs/circle/addon/wlan/firmware
    make
    cd ..
    cp -r firmware ${RELEASE}/Pi-Bootpart
    echo "console=serial0,115200 socmaxtemp=75 logdev=ttyS1 loglevel=2" > ${RELEASE}/Pi-Bootpart/cmdline.txt
    cat > ${RELEASE}/Pi-Bootpart/wpa_supplicant.conf <<EOF
#
# wpa_supplicant.conf
#
# adjust your country code
country=AT

network={
    # adjust your SSID
    ssid="REPLACE_WITH_MY_NETWORK_SSI"
    # adjust your WiFi password
    psk="REPLACYE_WITH_MY_WIFI_PASSWORD"
    proto=WPA2
    key_mgmt=WPA-PSK
}
EOF
    echo "fetching roms..."
    cd ${RELEASE}/Pi-Bootpart
    rm dos*.bin
    wget https://sourceforge.net/p/vice-emu/code/HEAD/tree/trunk/vice/data/DRIVES/dos1541-325302-01%2B901229-05.bin?format=raw -O dos1541
    wget https://sourceforge.net/p/vice-emu/code/HEAD/tree/trunk/vice/data/DRIVES/dos1541ii-251968-03.bin?format=raw -O dos1541ii
    wget https://sourceforge.net/p/vice-emu/code/HEAD/tree/trunk/vice/data/DRIVES/dos1581-318045-02.bin?format=raw -O dos1581
    rm chargen-906143-02.bin
    wget https://sourceforge.net/p/vice-emu/code/HEAD/tree/trunk/vice/data/C64/chargen-906143-02.bin?format=raw -O chargen
    wget https://www.nightfallcrew.com/wp-content/plugins/download-monitor/download.php?id=48 -O /tmp/jiffy.zip
    unzip /tmp/jiffy.zip JiffyDOS_1541-II.bin JiffyDOS_1581.bin
    rm /tmp/jiffy.zip

    # finally populate options.txt and config.txt
    cd ${base}
    cp options.txt config.txt ${RELEASE}/Pi-Bootpart
    mkdir ${RELEASE}/Pi-Bootpart/1541
    wget https://cbm-pi1541.firebaseapp.com/fb.d64 -O ${RELEASE}/Pi-Bootpart/1541/fb.d64
    echo "populated a Pi bootpartition for pottendo-Pi1541:"
    ls -l ${RELEASE}/Pi-Bootpart
    echo "Don't forget to adapt 'options.txt' and 'wpa_supplicant.conf' to your local needs!"
    exit 0
fi

echo "building pottendo-Pi1541 for ${archs}"
for a in ${archs} ; do
    cd ${CIRCLE}
    case "$a" in
	pi3-32)
	    opts="-r 3"
	    ;;
	pi3-64)
	    opts="-r 3 -p aarch64-none-elf-"
	    ;;
	pi4-32)
	    opts="-r 4"
	    ;;
	pi4-64)
	    opts="-r 4 -p aarch64-none-elf-"
	    ;;
	pi5-64)
	    opts="-r 5 -p aarch64-none-elf- -o SERIAL_DEVICE_DEFAULT=0 -o SCREEN_HEADLESS"
	    ;;
	*)
	    echo "unkown arch ${a}"
	    exit 1
	    ;;
    esac
    make mrproper 2>&1 > /dev/null
    echo "configuring circle-stdlib: ${opts}..."
    ./configure $opts >make-${a}.log
    sed -i 's/CFLAGS_FOR_TARGET =/CFLAGS_FOR_TARGET = -O3/g' Config.mk
    sed -i 's/CPPFLAGS_FOR_TARGET =/CPPFLAGS_FOR_TARGET = -O3/g' Config.mk
    echo "building circle-stdlib, may take a while..."
    if make -j12 2>&1 >>make-${a}.log; then
	    echo "success fully built circle: ${a} ${opts}"
    else
	    echo "fail for ${a}"
	    exit 1
    fi
    cd ${PPI1541}
    make clean 2>&1 > /dev/null
    echo "building pottendo-Pi1541..."
    if make -j12 >>make-${a}.log; then
	    echo "success fully built pottendo-Pi1541: ${a} ${opts}"
    else
	    echo "fail for ${a}"
	exit 1
    fi
    cp kernel*.img ${RELEASE}
done
ls -l ${RELEASE}
echo "successfully built for ${archs}"
