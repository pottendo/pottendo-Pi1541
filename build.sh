#!/bin/sh

base=`pwd`
if git remote get-url origin | grep `basename ${base}` ; then
    echo "Running in ${base}, OK!"
else
    echo "$base should be the checkout pottendo-Pi1541"
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

if [ x${checkout} = "xyes" ] ; then
    cd ${base}/..
    rm -rf ${CIRCLE}
    git clone --recursive https://github.com/smuehlst/circle-stdlib.git
    cd ${CIRCLE}/libs/circle
    patch -p1 < ../../../pottendo-Pi1541/src/Circle/patch-circle-V49.0.diff
    exit
fi

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
    ./configure $opts
    sed -i 's/CFLAGS_FOR_TARGET =/CFLAGS_FOR_TARGET = -O3/g' Config.mk
    sed -i 's/CPPFLAGS_FOR_TARGET =/CPPFLAGS_FOR_TARGET = -O3/g' Config.mk
    if make -j12 >make-${a}.log; then
	echo "success fully built circle: ${a} ${opts}"
    else
	echo "fail for ${a}"
	exit 1
    fi
    cd ${PPI1541}
    make clean 2>&1 > /dev/null
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
