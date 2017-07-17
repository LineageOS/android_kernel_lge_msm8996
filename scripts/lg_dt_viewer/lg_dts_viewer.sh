#guilbert.lee@lge.com Mon 28 Jan 2013
#lg dts viewer

#!/bin/sh

DTS_PATH=$1
WORK_PATH=${0%/*}
FILTER="scripts"
KERNEL_PATH=${PWD%$FILTER*}

usage() {
    echo "usage : ./lg_dts_viewer.sh [dts file] | [dts path]"

    echo "    - dts path : path of dts file"
    echo "    - dts file : dts file (with full path)"
    echo
    echo "example (in Kernel root) :"
    echo "./scripts/lg_dt_viewer/lg_dts_viewer.sh arch/arm/boot/dts/lge/msm8994-h1_kr/msm8994-h1_kr_rev-a.dts"
    echo "or"
    echo "./scripts/lg_dt_viewer/lg_dts_viewer.sh arch/arm/boot/dts/lge/msm8994-h1_kr/"
    exit
}

while getopts dh flag; do
    case $flag in
    d)
        # for debugging(hidden)
        shift 1
        DEBUG=debug
        DTS_PATH=$1
        ;;
    h)
        usage
        ;;
    \?)
        usage
        ;;
    esac
done

if [ "$DTS_PATH" = "" ]; then
    usage
fi

DTS_NAME=${DTS_PATH##*/}

if [ "$DTS_NAME" = "" ]; then
    if [ ! -d "$DTS_PATH" ]; then
        echo "ERROR: directory not found"
        exit
    fi
    echo "NOTE: processing path including dts, searching dts files..."
    OUT_PATH=$(basename $DTS_PATH)
    OUT_PATH=out_$OUT_PATH
    DTS_PATH=$(find $DTS_PATH -name *.dts)
    if [ "$DTS_PATH" = "" ]; then
        echo "ERROR: There is no dts file in this path"
        exit
    fi
else
    echo "NOTE: processing indivisual dts file..."
    case "$DTS_NAME" in
    *.dts)
        ;;
    *)
        echo "NOTE: Input file is not dts file"
        exit
        ;;
    esac
    OUT_PATH=${DTS_NAME/%.dts/}
    OUT_PATH=out_$OUT_PATH
fi

if [ ! -d "$OUT_PATH" ] ; then
    mkdir $OUT_PATH
fi

for dtspath in $DTS_PATH; do
    echo "DTC $dtspath"
    dtsfile=${dtspath##*/}
    # For support #include, C-pre processing first to dts.
    gcc -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
		-I $KERNEL_PATH/arch/arm/boot/dts/ \
		-I $KERNEL_PATH/arch/arm/boot/dts/include/ \
		$dtspath -o $OUT_PATH/$dtsfile.preprocessing

    # Now, transfer to dts from dts with LG specific
    ${WORK_PATH}/lg_dtc -o $OUT_PATH/$dtsfile\
        -D -I dts -O dts -H specific -s ./$OUT_PATH/$dtsfile.preprocessing

    if [ "$DEBUG" = "debug" ]
    then
        ${WORK_PATH}/lg_dtc -o $OUT_PATH/$dtsfile.2\
           -D -I dts -O dts -H specific2 -s ./$OUT_PATH/$dtsfile.preprocessing
    else
        rm ./$OUT_PATH/$dtsfile.preprocessing
    fi
done
echo "out is ./$OUT_PATH"
echo "done."
