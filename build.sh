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
    -l)
        build_legacy="only"
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
    mkdir -p ${RELEASE}/debug-syms 2>/dev/null
    build_legacy="yes"
else
    # install in builddir, where the checkouts have been done
    RELEASE=${base}/../Pi-Bootpart
    mkdir -p ${RELEASE}/debug-syms 2>/dev/null
fi

if [ x${build_legacy} = "xyes" -o x${build_legacy} = "xonly" ] ; then
    builds="0 2 3 1BPlus 1BRev1 1BRev2"
    mkdir ${RELEASE}/orig-build 2>/dev/null
    for b in ${builds} ; do
        make RASPPI=${b} clean
        cd ${PPI1541}
        echo "building legacy codebase for RASPPI=${b}..."
        if make RASPPI=${b} legacy > make-RASPPI-${b}-legacy.log; then
            echo "successully built for RASPPI=${b}, legacy code base"
            mv kernel.img ${RELEASE}/orig-build/kernel-Pi${b}.img
            cp kernel.lst ${RELEASE}/debug-syms/kernel-Pi${b}.lst
            cp kernel.map ${RELEASE}/debug-syms/kernel-Pi${b}.map
        else
            echo "failed to build legacy codebase for RASPPI=${b}"
            exit 1
        fi
    done
    make RASPPI=${b} clean
    cat > ${RELEASE}/orig-build/README.txt <<EOF
# Legacy builds of pottendo-Pi1541
# These builds are based on the original codebase
# prior to migration to circle-stdlib.
# They are provided for compatibility reasons
# and for older Raspberry Pi models.
# The builds are provided as-is, without
# any support or warranty.
#
# place appropriate files in the bootpartition of your Raspberry Pi SD card
# to use them on the respective Raspberry Pi models:
# kernel-Pi0.img      -> Raspberry Pi Zero, Pi Zero W
# kernel-Pi1BPlus.img -> Raspberry Pi 1 Model B+, 40 pin GPIO
# kernel-Pi1BRev1.img -> Raspberry Pi 1 Model B Revision 1, 26 pin GPIO
# kernel-Pi1BRev2.img -> Raspberry Pi 1 Model B Revision 2, 26 pin GPIO
# kernel-Pi2.img      -> Raspberry Pi 2
# kernel-Pi3.img      -> Raspberry Pi 3, Pi 3B+, Pi 3A+, Pizero 2 W
# verify config.txt for correct kernel file names!
EOF
    echo "successfully built legacy codebase for all RASPPI models"
    if [ x${build_legacy} = "xonly" ] ; then
        echo "only legacy build requested, exiting."
        exit 0
    fi
fi

if [ x${checkout} = "xyes" ] ; then
    cd ${base}/..
    rm -rf ${CIRCLE}
    git clone --recursive https://codeberg.org/larchcone/circle-stdlib.git
    cd ${CIRCLE}/libs/circle
    patch -p1 < ../../../pottendo-Pi1541/src/Circle/patch-circle-V50.1.diff
    # fetch bootfiles for RPis
    cd ${CIRCLE}/libs/circle/boot
    make
    cp LICENCE.broadcom bootcode.bin fixup* start* bcm2711-rpi-4-b.dtb ${RELEASE}
    # fetch WiFi firmware
    cd ${CIRCLE}/libs/circle/addon/wlan/firmware
    make
    cd ..
    cp -r firmware ${RELEASE}
    rm -f ${RELEASE}/firmware/.gitignore ${RELEASE}/firmware/Makefile
    echo "console=serial0,115200 socmaxtemp=80 logdev=ttyS1 loglevel=2" > ${RELEASE}/cmdline.txt
    cat > ${RELEASE}/wpa_supplicant.conf <<EOF
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
    cd ${RELEASE}
    rm dos*.bin
    wget https://sourceforge.net/p/vice-emu/code/HEAD/tree/trunk/vice/data/DRIVES/dos1541-325302-01%2B901229-05.bin?format=raw -O dos1541
    wget https://sourceforge.net/p/vice-emu/code/HEAD/tree/trunk/vice/data/DRIVES/dos1541ii-251968-03.bin?format=raw -O dos1541ii
    wget https://sourceforge.net/p/vice-emu/code/HEAD/tree/trunk/vice/data/DRIVES/dos1581-318045-02.bin?format=raw -O dos1581
    rm chargen-906143-02.bin
    wget https://sourceforge.net/p/vice-emu/code/HEAD/tree/trunk/vice/data/C64/chargen-906143-02.bin?format=raw -O chargen

    # finally populate options.txt and config.txt
    cd ${base}
    cp options.txt config.txt ${RELEASE}
    cp -r src/webcontent/web ${RELEASE}
    mkdir ${RELEASE}/1541
    wget https://cbm-pi1541.firebaseapp.com/fb.d64 -O ${RELEASE}/1541/fb.d64
    cd CBM-FileBrowser_v1.6/sources
    make clean 2>&1 > /dev/null
    echo "building CBM-FileBrowser_v1.6..."
    if make 2>&1 | tee make-CBM-FileBrowser.log; then
        echo "successfully built CBM-FileBrowser_v1.6"
        cp CBM-FileBrowser.d64 ${RELEASE}/1541/CBM-FileBrowser.d64  
    else
        echo "WARNING: failed to build CBM-FileBrowser_v1.6"
    fi
    echo "populated a Pi bootpartition for pottendo-Pi1541:"
    ls -l ${RELEASE}
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
    ./configure $opts 2>&1 >make-${a}.log
    sed -i 's/CFLAGS_FOR_TARGET =/CFLAGS_FOR_TARGET = -O3/g' Config.mk
    sed -i 's/CPPFLAGS_FOR_TARGET =/CPPFLAGS_FOR_TARGET = -O3/g' Config.mk
    echo "building circle-stdlib, may take a while..."
    if make -j12 2>&1 >>make-${a}.log; then
	    echo "successfully built circle: ${a} ${opts}"
    else
	    echo "fail for ${a}"
	    exit 1
    fi
    cd ${PPI1541}
    make clean 2>&1 > /dev/null
    echo "building pottendo-Pi1541..."
    if make -j12 >>make-${a}.log; then
	    echo "successfully built pottendo-Pi1541: ${a} ${opts}"
    else
	    echo "fail for ${a}"
	exit 1
    fi
    cp kernel*.img ${RELEASE}
    cp src/kernel*.lst ${RELEASE}/debug-syms
    cp src/kernel*.map ${RELEASE}/debug-syms
done
cp README.md ${RELEASE}
ls -l ${RELEASE}
echo "successfully built for ${archs}"
