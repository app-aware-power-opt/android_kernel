#!/bin/bash

CPU_JOB_NUM=$(grep processor /proc/cpuinfo | awk '{field=$NF};END{print field+1}')
CLIENT=$(whoami)

ROOT_DIR=$(pwd)
#KERNEL_DIR=../android-kernel-samsung-dev

#SEC_PRODUCT='generic' #Enable for generic build
SEC_PRODUCT=$1

IMAGE_OUT_DIR="$ROOT_DIR/$SEC_PRODUCT-img"
USES_EMMC=$(grep BOARD_USES_EMMC device/hardkernel/$SEC_PRODUCT/BoardConfig.mk | awk '{field=$NF};END{print field}')
WIFI_MODULE=$(grep "BOARD_WLAN_DEVICE	" device/hardkernel/$SEC_PRODUCT/BoardConfig.mk | awk '{field=$NF};END{print field}')

if [ $USES_EMMC = "true" ]
then
	if [ $WIFI_MODULE = "rt5370sta" ]
	then
		IMAGE_OUT_DIR="$ROOT_DIR/$SEC_PRODUCT-img_eMMC_rt5370sta"
	fi
	if [ $WIFI_MODULE = "rtl8191su" ]
	then
		IMAGE_OUT_DIR="$ROOT_DIR/$SEC_PRODUCT-img_eMMC_rtl8191su"
	fi
fi
if [ $USES_EMMC = "false" ]
then
	if [ $WIFI_MODULE = "rt5370sta" ]
	then
		IMAGE_OUT_DIR="$ROOT_DIR/$SEC_PRODUCT-img_SD_rt5370sta"
	fi
	if [ $WIFI_MODULE = "rtl8191su" ]
	then
		IMAGE_OUT_DIR="$ROOT_DIR/$SEC_PRODUCT-img_SD_rtl8191su"
	fi
fi

OUT_DIR="$ROOT_DIR/out/target/product/$SEC_PRODUCT"
OUT_HOSTBIN_DIR="$ROOT_DIR/out/host/linux-x86/bin"

function check_exit()
{
	if [ $? != 0 ]
	then
		exit $?
	fi
}

function build_android()
{
	echo
	echo '[[[[[[[ Build android platform ]]]]]]]'
	echo

	START_TIME=`date +%s`
	if [ $SEC_PRODUCT = "generic" ]
	then
		echo make -j$CPU_JOB_NUM
		echo
		make -j$CPU_JOB_NUM
	else
		echo make -j$CPU_JOB_NUM PRODUCT-$SEC_PRODUCT-eng
		echo
		make -j$CPU_JOB_NUM PRODUCT-$SEC_PRODUCT-eng 2>&1|tee build.log
	fi
	check_exit

	END_TIME=`date +%s`
	let "ELAPSED_TIME=$END_TIME-$START_TIME"
	echo "Total compile time is $ELAPSED_TIME seconds"
}

function make_uboot_img()
{
	cd $OUT_DIR

	echo
	echo '[[[[[[[ Make ramdisk image for u-boot ]]]]]]]'
	echo

	mkimage -A arm -O linux -T ramdisk -C none -a 0x40800000 -n "ramdisk" -d ramdisk.img ramdisk-uboot.img
	check_exit

	rm -f ramdisk.img

	echo
	cd ../../../..
}

function make_fastboot_img()
{
	echo
	echo '[[[[[[[ Make additional images for fastboot ]]]]]]]'
	echo

	if [ ! -f $KERNEL_DIR/arch/arm/boot/zImage ]
	then
		echo "No zImage is found at $KERNEL_DIR/arch/arm/boot"
		echo '  Please set KERNEL_DIR if you want to make additional images'
		echo "  Ex.) export KERNEL_DIR=~ID/android_kernel_$SEC_PRODUCT"
		echo
		return
	fi

	echo 'boot.img ->' $OUT_DIR
	cp $KERNEL_DIR/arch/arm/boot/zImage $OUT_DIR/zImage
	$OUT_HOSTBIN_DIR/mkbootimg --kernel $OUT_DIR/zImage --ramdisk $OUT_DIR/ramdisk-uboot.img -o $OUT_DIR/boot.img
	check_exit

	echo 'update.zip ->' $OUT_DIR
	zip -j $OUT_DIR/update.zip $OUT_DIR/android-info.txt $OUT_DIR/boot.img $OUT_DIR/system.img
	check_exit

	echo
}

function copy_output_data()
{
	echo 
	echo '[[[[[[[ OUTPUT FOLDER = '$IMAGE_OUT_DIR' ]]]]]]]'
	echo

	mkdir -p $IMAGE_OUT_DIR
	rm -rf $IMAGE_OUT_DIR/*
	
	cp -a $OUT_DIR/ramdisk-uboot.img $IMAGE_OUT_DIR
	cp -a $OUT_DIR/system.img $IMAGE_OUT_DIR
	cp -a $OUT_DIR/system $IMAGE_OUT_DIR
	cd $IMAGE_OUT_DIR

	# deleted .svn .git folder
	find . -type d -name .svn -print0 | xargs -0 rm -rf
	find . -type d -name .git -print0 | xargs -0 rm -rf

	chmod 777 -R *
	sync
}

echo
echo '                Build android for '$SEC_PRODUCT''
echo

case "$SEC_PRODUCT" in
	smdkv310)
		build_android
		make_uboot_img
		make_fastboot_img
		;;
	smdk4x12)
		build_android
		make_uboot_img
		make_fastboot_img
		;;
    odroidq)
		build_android
		make_uboot_img
		copy_output_data
		make_fastboot_img
		;;
	odroidq2)
		build_android
		make_uboot_img
		copy_output_data
		make_fastboot_img
		;;
    odroida)
		build_android
		make_uboot_img
		copy_output_data
		make_fastboot_img
		;;
    odroida4)
		build_android
		make_uboot_img
		copy_output_data
		make_fastboot_img
		;;
    odroidpc)
		build_android
		make_uboot_img
		copy_output_data
		make_fastboot_img
		;;
	odroidx)
		build_android
		make_uboot_img
		copy_output_data
		make_fastboot_img
		;;
	odroidu)
		build_android
		make_uboot_img
		copy_output_data
		make_fastboot_img
		;;
	odroidu2)
		build_android
		make_uboot_img
		copy_output_data
		make_fastboot_img
		;;
	odroidx2)
		build_android
		make_uboot_img
		copy_output_data
		make_fastboot_img
		;;
	smdk5250)
		build_android
		make_uboot_img
		make_fastboot_img
		;;
	generic)
		build_android
		make_uboot_img
		;;
	*)
		echo "Please, set SEC_PRODUCT"
		echo "  export SEC_PRODUCT=smdkv310 or SEC_PRODUCT=smdk4x12 or SEC_PRODUCT=smdk5250 SEC_PRODUCT=odroidq SEC_PRODUCT=odroidq2 SEC_PRODUCT=odroida SEC_PRODUCT=odroidpc SEC_PRODUCT=odroida4"
		echo "     SEC_PRODUCT=odroidx SEC_PRODUCT=odroidq SEC_PRODUCT=odroidq2 SEC_PRODUCT=odroidx2 SEC_PRODUCT=odroidu SEC_PRODUCT=odroidu2 or "
		echo "  export SEC_PRODUCT=generic"
		exit 1
		;;
esac

echo ok success !!!

exit 0
