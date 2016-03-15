Patch files for qcserial are here.
To apply patch to your kernel, use something like this in your kernel directory :
# patch -p0 < ../qcserial-htc-one-m7.patch

Then rebuild the qcserial module :
# make modules
or
# make drivers/usb/serial/qcserial.ko

Then, insert module with
# insmod  drivers/usb/serial/qcserial.ko
or use modprobe if you choose to install it to your system (bad idea imo).

It might require another module called usb-wwan, just modprobe it, this one doesn't need any patch.
