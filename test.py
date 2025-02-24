import usb.core
import usb.util

# Find our device
dev = usb.core.find(idVendor=0x1430, idProduct=0x0150)
if dev is None:
    raise ValueError('Device not found')
try:
    dev.detach_kernel_driver(0)
except:
    pass
# dev.ctrl_transfer(0x21, 9, wValue=0x0200, wIndex=0, data_or_wLength=b"C\xff\xff\xff", timeout=None)
dev.ctrl_transfer(0x21, 9, wValue=0x0200, wIndex=0, data_or_wLength=b"J\x00\x20\x20\x20\xff\xff", timeout=None)
