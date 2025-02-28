import usb.core
import usb.util
import time

# Find our device
dev = usb.core.find(idVendor=0x1430, idProduct=0x0150)
if dev is None:
    raise ValueError('Device not found')
try:
    dev.detach_kernel_driver(0)
except:
    pass
dev.ctrl_transfer(0x21, 9, wValue=0x0200, wIndex=0, data_or_wLength=b"C\xff\x00\x00", timeout=None)
time.sleep(0.001)
dev.ctrl_transfer(0x21, 9, wValue=0x0200, wIndex=0, data_or_wLength=b"J\x00\x00\xff\x00\x00\x30", timeout=None)

time.sleep(5)
dev.ctrl_transfer(0x21, 9, wValue=0x0200, wIndex=0, data_or_wLength=b"J\x00\x00\x00\xff\x00\x10", timeout=None)