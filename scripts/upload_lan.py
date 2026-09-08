"""PlatformIO custom uploader for MKR Zero + SDU. Uses only Python's stdlib."""
import argparse
from pathlib import Path
import re
import socket
import struct
import sys
import zlib


def validate_image(data):
    if not 0x4008 <= len(data) <= 0x3E000:
        raise ValueError("Image does not fit MKR Zero SDU layout")
    sp, reset = struct.unpack_from("<II", data, 0x4000)
    if not (0x20000000 < sp <= 0x20008000 and sp % 8 == 0
            and reset & 1 and 0x6001 <= reset < 0x2000 + len(data)):
        raise ValueError("Missing MKR Zero SDU application vectors")


def upload(host, data, token, port=65280):
    validate_image(data)
    if not re.fullmatch(r"[0-9a-fA-F]{32,64}", token):
        raise ValueError("GDC_UPLOAD_TOKEN must contain 32-64 hexadecimal characters")
    with socket.create_connection((host, port), timeout=15) as connection:
        connection.settimeout(180)
        with connection.makefile("rb") as response:
            header = f"GDC1 {token} {len(data)} {zlib.crc32(data):08x}\n"
            connection.sendall(header.encode("ascii"))
            ready = response.readline(256).decode("ascii", errors="replace").strip()
            legacy_server = ready.startswith("WAIT ")
            if legacy_server:
                print("Controller uses the previous updater; approve its existing prompt to install this compatibility update.", flush=True)
                ready = response.readline(256).decode("ascii", errors="replace").strip()
            if ready != "READY":
                raise RuntimeError(ready or "Controller closed the connection")
            connection.sendall(data)
            result = response.readline(256).decode("ascii", errors="replace").strip()
            if legacy_server and result == "OK staged; rebooting":
                print("Firmware accepted by the previous updater; controller is rebooting.")
                return
            if result != "STAGED awaiting local approval":
                raise RuntimeError(result or "Upload interrupted; no confirmation received")
    print("Firmware verified on SD. Approve it with INFO on the controller within 60 seconds.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--firmware", required=True, type=Path)
    args = parser.parse_args()
    if not args.host:
        parser.error("Set GDC_HOST to the controller's DHCP address before starting VS Code/PlatformIO")
    config = Path(__file__).resolve().parents[1] / "include/config_local.h"
    match = re.search(r'^\s*#define\s+GDC_UPLOAD_TOKEN\s+"([0-9a-fA-F]{32,64})"',
                      config.read_text(encoding="utf-8"), re.MULTILINE)
    if not match:
        raise ValueError("Configure GDC_UPLOAD_TOKEN in include/config_local.h and USB-upload once")
    upload(args.host, args.firmware.read_bytes(), match.group(1))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError) as error:
        print(f"LAN upload failed: {error}", file=sys.stderr)
        sys.exit(1)
