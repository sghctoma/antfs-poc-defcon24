#!/usr/bin/env python

import os
import sys
import struct

class BinaryReader:
    def __init__(self, path):
        self._stream = open(path, "rb")
        self._stream.seek(0, os.SEEK_END)
        self.length = self._stream.tell()
        self._stream.seek(0, os.SEEK_SET)

    def read_char(self):
        return struct.unpack("<B", self._stream.read(1))[0]

    def read_uint16(self):
        return struct.unpack("<H", self._stream.read(2))[0]

    def read_uint32(self):
        return struct.unpack("<I", self._stream.read(4))[0]

    def __getattr__(self, name):
        return getattr(self._stream, name)

class GarminFirmware:
    _magic = 0x7247704b # KpGr

    def __init__(self, path):
        if path:
            self._reader = BinaryReader(path)

    def _has_wrapper(self):
        m = self._reader.read_uint32()
        return m == self._magic

    def _valid(self):
        if self._reader.read_uint16() <> 100:
            return False

        num = self._reader.read_uint32()
        if self._reader.read_char() <> 68:
            return False

        if self._reader.read_char() <> 100:
            return False

        self._reader.read(num - 1)

        return True

    def strip(self, ant = True):
        if not self._has_wrapper():
            raise ValueError("Firmware file is not wrapped.")

        if not self._valid():
            raise ValueError("Firmware file is invalid.")

        while self._reader.tell() < self._reader.length:
            num2 = self._reader.read_uint32()
            if self._reader.read_char() <> 82:
                p = self._reader.tell() + num2
                self._reader.seek(p, os.SEEK_SET)
            else:
                num1 = self._reader.read_uint16()
                num3 = self._reader.read_uint32()
                num4 = self._reader.read_uint32()
                if num1 == 12 and not ant:
                    p = self._reader.tell() + num4
                    self._reader.seek(p, os.SEEK_SET)
                else:
                    buffer = self._reader.read()
                    break

        self._reader.close()

        if num1 == -1:
            raise ValueError("Could not find region code in firmware header.")

        return num1, buffer

def main(argv):
    fw = GarminFirmware(argv[0])
    result, stripped = fw.strip()
    print result
    print len(stripped)
    with open(argv[0] + ".stripped", "wb") as fd:
        fd.write(stripped)

if __name__ == "__main__":
    main(sys.argv[1:])

