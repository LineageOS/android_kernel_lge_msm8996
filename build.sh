
# root directory of this kernel (this script's location)
RDIR=$(pwd)

# build dir
BDIR=build

ZDIR=out

export ARCH=arm64
export CROSS_COMPILE=$HOME/build/toolchain/lineageos/android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-android-4.9/bin/aarch64-linux-android-
export CROSS_COMPILE_ARM32=$HOME/build/toolchain/lineageos/android_prebuilts_gcc_linux-x86_arm_arm-linux-androideabi-4.9/bin/arm-linux-androideabi-
##############G5############################
mkdir -p "$BDIR" 
mkdir -p "$ZDIR"
##############G5############################

make -C "$RDIR" O="$BDIR"  lineageos_rs988_defconfig
make -j4 -C "$RDIR" O="$BDIR" 

##############G5############################
rm $ZDIR/anykernel/Image.lz4-dtb
cp $BDIR/arch/arm64/boot/Image.lz4-dtb $ZDIR/anykernel/Image.lz4-dtb

cd $ZDIR
cd anykernel
zip -7qr $RDIR/out/kernel-LOS.zip * \

