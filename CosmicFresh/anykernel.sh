# AnyKernel3 Ramdisk Mod Script
# osm0sis @ xda-developers

## AnyKernel setup
# begin properties
properties() { '
do.devicecheck=0
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=a51
device.name2=
device.name3=
device.name4=
device.name5=
supported.versions=11.0.0-12.0.0
supported.patchlevels=
'; } # end properties

# shell variables
block=/dev/block/platform/13520000.ufs/by-name/boot;
dtboblock=/dev/block/platform/13520000.ufs/by-name/dtbo;
is_slot_device=0;
ramdisk_compression=auto;

## AnyKernel methods (DO NOT CHANGE)
# import patching functions/variables - see for reference
. tools/ak3-core.sh;

## Trim partitions
$bin/busybox fstrim -v /data;

## AnyKernel boot install
dump_boot;

write_boot;
## end boot install

# Vendor boot
#block=vendor_boot;
#is_slot_device=1;
#ramdisk_compression=auto;

# reset for vendor_boot patching
#reset_ak;

## AnyKernel vendor_boot install
#split_boot;

#flash_boot;
## end vendor_boot install
