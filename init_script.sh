#!/bin/sh
module="mailbox"
device="mailbox"
mode="666"
dev_number="255"

# clean old module and invoke insmod with all arguments we were passed
# and use a pathname, as newer modutils don't look in . by default
/sbin/rmmod $module ; /sbin/insmod -f ./$module.ko $* || exit 1

# remove stale nodes
for i in `seq 0 $dev_number`;
do
    rm -f /dev/${device}$i
done


major=`awk "\\$2==\"$module\" {print \\$1}" /proc/devices`

# create device and give appropriate group/permissions, and change the group.
# Not all distributions have staff; some have "wheel" instead.
group="staff"
grep '^staff:' /etc/group > /dev/null || group="wheel"

for i in `seq 0 $dev_number`;
do
    mknod /dev/${device}$i c $major $i
    chgrp $group /dev/${device}$i
    chmod $mode /dev/${device}$i
done

gcc ioctl.c -o ioctl
#mknod /dev/${device}0 c $major 0
#mknod /dev/${device}1 c $major 1
#mknod /dev/${device}2 c $major 2
#mknod /dev/${device}3 c $major 3
