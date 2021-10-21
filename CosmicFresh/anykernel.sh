# AnyKernel3 Ramdisk Mod Script
# osm0sis @ xda-developers
# Edit by @Bode327 for A30
## AnyKernel setup
## Thnx to @physwizz @Chatur27 for their great work Also @Joel 
# begin properties
properties() { '
kernel.string=
do.devicecheck=0
do.modules=0
do.cleanup=1
do.cleanuponabort=0
device.name1=
device.name2=
device.name3=
device.name4=
device.name5=
supported.versions=
'; } # end properties

# shell variables
block=/dev/block/platform/13520000.ufs/by-name/boot;
is_slot_device=0;
ramdisk_compression=auto;

## AnyKernel methods (DO NOT CHANGE)
# import patching functions/variables - see for reference
. tools/ak3-core.sh;

ui_print "- Unpacking boot image";

## AnyKernel install
dump_boot;

#Enable Spectrum Support
#ui_print "- Setting Up Spectrum";
#replace_file init.spectrum.rc 644 init.spectrum.rc;
#replace_file init.spectrum.sh 755 init.spectrum.sh;
#insert_line init.rc "/import init.spectrum.rc" after "/import init.container.rc"  "/import init.spectrum.rc";

## AnyKernel file attributes
# set permissions/ownership for included ramdisk files // Test
set_perm_recursive 0 0 755 644 $ramdisk/*;
set_perm_recursive 0 0 750 750 $ramdisk/init* $ramdisk/sbin;

mount -o rw,remount -t auto /system >/dev/null;
rm -rf /system/bin/vaultkeeper;
rm -rf /system/etc/tima;
rm -rf /system/etc/init/secure_storage_daemon.rc;
rm -rf /system/lib/libvkjni.so;
rm -rf /system/lib/libvkservice.so;
rm -rf /system/lib64/libvkjni.so;
rm -rf /system/lib64/libvkservice.so;
rm -rf /system/priv-app/KLMSAgent;
rm -rf /system/priv-app/KnoxGuard;
rm -rf /system/priv-app/Rlc;
rm -rf /system/priv-app/TeeService;
rm -rf /system/vendor/app/mcRegistry/ffffffffd0000000000000000000000a.tlbin;

# Fix secure storage (Wi-Fi)
cp /tmp/anykernel/tools/libsecure_storage.so /system/vendor/lib/libsecure_storage.so;
chmod 644 /system/vendor/lib/libsecure_storage.so;
cp /tmp/anykernel/tools/libsecure_storage_jni.so /system/vendor/lib/libsecure_storage_jni.so;
chmod 644 /system/vendor/lib/libsecure_storage_jni.so;

# Make init.d path if non-existent
ui_print "Initializing init.d support...";
mkdir /system/etc/init.d;
chmod 755 /system/etc/init.d;

# Change permissions
chmod 755 /system/bin/busybox;

# Optimized_Kernel_Features
cp /tmp/anykernel/tools/sysinit_cm /system/bin/sysinit_cm;
chmod 644 /system/system/bin/sysinit_cm;
cp /tmp/anykernel/tools/30zram /system/etc/init.d/30zram;
cp /tmp/anykernel/tools/40perf /system/etc/init.d/40perf;

####################################################################
# Set KNOX to 0x0 on running /system
$RESETPROP ro.boot.warranty_bit "0";
$RESETPROP ro.warranty_bit "0";

# Fix Samsung Related Flags
$RESETPROP ro.fmp_config "1";
$RESETPROP ro.boot.fmp_config "1";

# Fix Samsung Health (CuBz90@XDA)
$RESETPROP ro.config.tima "0";

# Fix safetynet flags
$RESETPROP ro.boot.veritymode "enforcing";
$RESETPROP ro.boot.verifiedbootstate "green";
$RESETPROP ro.boot.flash.locked "1";
$RESETPROP ro.boot.ddrinfo "00000001";

# Google play services wakelock fix (@Tkkg1994)
sleep 1
su -c "pm enable com.google.android.gms/.update.SystemUpdateActivity";
su -c "pm enable com.google.android.gms/.update.SystemUpdateService";
su -c "pm enable com.google.android.gms/.update.SystemUpdateService$ActiveReceiver";
su -c "pm enable com.google.android.gms/.update.SystemUpdateService$Receiver";
su -c "pm enable com.google.android.gms/.update.SystemUpdateService$SecretCodeReceiver";
su -c "pm enable com.google.android.gsf/.update.SystemUpdateActivity";
su -c "pm enable com.google.android.gsf/.update.SystemUpdatePanoActivity";
su -c "pm enable com.google.android.gsf/.update.SystemUpdateService";
su -c "pm enable com.google.android.gsf/.update.SystemUpdateService$Receiver";
su -c "pm enable com.google.android.gsf/.update.SystemUpdateService$SecretCodeReceiver";

# Deepsleep fix (@Chainfire)
for i in `ls /sys/class/scsi_disk/`; do
	cat /sys/class/scsi_disk/$i/write_protect 2>/dev/null | grep 1 >/dev/null
	if [ $? -eq 0 ]; then
		echo 'temporary none' > /sys/class/scsi_disk/$i/cache_type
	fi
done;

umount /system;

ui_print "- Installing new boot image";

write_boot;

ui_print "- Done";
ui_print " ";

## end install
