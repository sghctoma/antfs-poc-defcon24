#!/usr/bin/env python3

import binascii
import json
import os
import sys


address_length = 5

'''
Helper functions
'''

def make_word(data):
    return data[1] << 8 | data[0]

def make_dword(data):
    return data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0]

'''
Client Beacon
'''
def parse_beacon(data):
    parsed = {"type": "client beacon"}

    status1 = data[0]
    parsed["data"] = True if (status1 & (1 << 5)) else False
    parsed["upload"] = True if (status1 & (1 << 4)) else False
    parsed["pairing"] = True if (status1 & (1 << 3)) else False
    parsed["period"] = {
            0: "0.5 Hz (65535)",
            1: "1 Hz (32768)",
            2: "2 Hz (16384)",
            3: "4 Hz (8192)",
            4: "8 Hz (4096)",
            7: "match established",
        }.get(status1 & 7, "reserved")

    parsed["state"] = {
            0: "link",
            1: "auth",
            2: "transport",
            3: "busy",
        }.get(data[1] & 0x0f, "reserved")

    parsed["auth_type"] = {
            0: "pass-through",
            1: "n/a",
            2: "pairing",
            3: "passkey & pairing",
        }.get(data[2], "reserved")

    if parsed["state"] == "link":
        parsed["device_type"] = make_word(data[3:])
        parsed["manufacturer"] = make_word(data[5:])
    elif parsed["state"] == "auth" or parsed["state"] == "transport":
        parsed["host_serial"] = make_dword(data[3:])

    return parsed

'''
ANTFS Commands
'''
def parse_link_cmd(data):
    parsed = {"type": "link command"}
    parsed["frequency"] = data[1]
    parsed["period"] = {
            0: "0.5 Hz (65535)",
            1: "1 Hz (32768)",
            2: "2 Hz (16384)",
            3: "4 Hz (8192)",
            4: "8 Hz (4096)",
            7: "match established",
        }.get(data[2], "reserved")
    parsed["host_serial"] = make_dword(data[3:])
    return parsed

def parse_disconnect_cmd(data):
    parsed = {"type": "disconnect command"}

    if data[1] == 0:
        parsed["disconnect_type"] = "return to link"
    elif data[1] == 1:
        parsed["disconnect_type"] = "return to broadcast"
    elif data[1] <= 127:
        parsed["disconnect_type"] = "reserved"
    else:
        parsed["disconnect_type"] = "device specific"

    parsed["time duration"] = data[2]
    parsed["application duration"] = data[3]
    return parsed

def parse_auth_cmd(data):
    parsed = {"type": "auth command"}
    parsed["auth_type"] = {
            0: "pass-through",
            1: "request serial",
            2: "request pairing",
            3: "request passkey",
        }.get(data[1], "EINVAL")
    parsed["auth_string_length"] = data[2]
    parsed["host_serial"] = make_dword(data[3:])
    return parsed

def parse_ping_cmd(data):
    parsed = {"type": "ping command"}
    return parsed

#XXX: 2-packet burst
def parse_download_req_cmd1(data):
    parsed = {"type": "download request command"}
    parsed["index"] = make_word(data[1:])
    parsed["offset"] = make_dword(data[3:])
    return parsed

#XXX: 2-packet burs
def parse_upload_req_cmd1(data):
    parsed = {"type": "upload request command"}
    parsed["index"] = make_word(data[1:])
    parsed["max_size"] = make_dword(data[3:])
    return parsed

#XXX: 2+n-packet burst
def parse_upload_data_cmd(data):
    parsed = {"type": "upload data command"}
    parsed["crc_seed"] = make_word(data[1:])
    parsed["offset"] = make_dword(data[3:])
    return parsed

def parse_erase_req_cmd(data):
    parsed = {"type": "erase request command"}
    parsed["index"] = make_word(data[1:])
    return parsed

'''
ANTFS Respoonses
'''
def parse_auth_resp(data):
    parsed = {"type": "auth response"}
    parsed["response"] = {
            0: "response to serial req.",
            1: "accept",
            2: "reject",
        }.get(data[1], "EINVAL")
    parsed["auth_string_length"] = data[2]
    parsed["client_serial"] = make_dword(data[3:])
    return parsed

#XXX: 3+n-packet burst
def parse_download_req_resp2(data):
    parsed = {"type": "download request response"}
    parsed["response"] = {
            0: "ANTFS_OK",
            1: "ANTFS_ENOENT",
            2: "ANTFS_EACCESS",
            3: "ANTFS_ENOTREADY",
            4: "ANTFS_EINVAL",
            5: "ANTFS_ECRC",
        }.get(data[1], "EINVAL")
    parsed["remaining"] = make_dword(data[3:])
    return parsed

#XXX: 4-packet burst
def parse_upload_req_resp2(data):
    parsed = {"type": "upload request response"}
    parsed["response"] = {
            0: "ANTFS_OK",
            1: "ANTFS_ENOENT",
            2: "ANTFS_EACCESS",
            3: "ANTFS_ENOSPC",
            4: "ANTFS_EINVAL",
            5: "ANTFS_ENOTREADY",
        }.get(data[1], "EINVAL")
    parsed["last_offset"] = make_dword(data[3:])
    return parsed

#XXX: 2-packet burst
def parse_upload_data_resp2(data):
    parsed = {"type": "upload data response"}
    parsed["response"] = data[1] is not None
    return parsed

#XXX: 2-packet burst
def parse_erase_resp2(data):
    parsed = {"type": "erase response"}
    parsed["response"] = {
            0: "OK",
            1: "FAILED",
            2: "ENOTREADY",
        }.get(data[1], "EINVAL")
    return parsed

'''
Parser
'''

def parse_unknown_cmdresp(data):
    parsed = {"type": "unknown command/response"}
    parsed["data"] = binascii.hexlify(bytearray(data))
    return parsed

def parse_cmdresp(data):
    parser = {
            # ANTFS Commands
            0x02: parse_link_cmd,
            0x03: parse_disconnect_cmd,
            0x04: parse_auth_cmd,
            0x05: parse_ping_cmd,
            0x09: parse_download_req_cmd1,
            0x0a: parse_upload_req_cmd1,
            0x0b: parse_erase_req_cmd,
            0x0c: parse_upload_data_cmd,
            # ANTFS Respoonses
            0x84: parse_auth_resp,
            0x89: parse_download_req_cmd1,
            0x8a: parse_upload_req_resp2,
            0x8b: parse_erase_resp2,
            0x8c: parse_upload_data_resp2,
        }.get(data[0], parse_unknown_cmdresp)
    return parser(data)

def parse(data):
    address = 0
    for i in range(address_length):
        address |= data[i] << (address_length - 1 - i) * 8
    data = data[address_length:]

    if data[2] == 0x43:
        parsed = parse_beacon(data[3:])
    elif data[2] == 0x44:
        parsed = parse_cmdresp(data[3:])
    else:
        parsed = {"type": "unknown"}
        parsed["address"] = address
        parsed["data"] = binascii.hexlify(bytearray(data))

    parsed["address"] = hex(address)
    return parsed

'''
main
'''

for line in sys.stdin:
    if not line:
        break;

    numbers = [int(x, 16) for x in line.split()]
    parsed = parse(numbers)
    print(parsed)
    #print(json.dumps(parsed))
