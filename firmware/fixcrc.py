#!/usr/bin/env python3

"""
Garmin firmwares' last byte is a very simple checksum: the sum of all the bytes
of the firmware should be 0, which essentially means that
last_byte = -(sum(previous_bytes)). This script calculates the correct value and
saves the modified firmware with a ".fixed" suffix.
"""

import sys
import struct

with open(sys.argv[1], 'rb') as f:
    firmware = f.read()

length = len(firmware)
fmt='%db' % length
fwbytes = struct.unpack(fmt, firmware)[0:-1]
cs = sum(fwbytes) % 256
cs = 256 - cs if cs > 127 else -1 * cs
print("Correct checksum: " + str(cs))

with open(sys.argv[1] + '.fixed', 'wb') as f:
    f.write(struct.pack(fmt, *(fwbytes + (cs,))))
