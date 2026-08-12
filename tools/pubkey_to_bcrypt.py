#!/usr/bin/env python3
# Copyright (C) 2025 Matthew Moran
#
# This file is part of ChartDisplay.  This program is free
# software; you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the
# Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Converts an RSA public key in PEM form into the raw BCRYPT_RSAKEY_BLOB that
# BCryptImportKeyPair expects, for embedding in ChartDisplay.rc as RCDATA.
#
#   ./pubkey_to_bcrypt.py signing-key.pub.pem ChartDisplay/update-pubkey.bin
#
# Then add to ChartDisplay.rc:
#
#   IDR_UPDATE_PUBKEY   RCDATA   "update-pubkey.bin"
#
# Only ever feed this the PUBLIC half. Signing stays on the machine holding the
# private key; nothing here needs it.

import re
import struct
import subprocess
import sys

# BCRYPT_RSAPUBLIC_MAGIC, 'RSA1' in little-endian bytes
RSA_PUBLIC_MAGIC = 0x31415352


def openssl(pem, *args):
    return subprocess.run(["openssl", "rsa", "-pubin", "-in", pem, *args],
                          capture_output=True, text=True, check=True).stdout


def build_blob(pem):
    # -modulus prints "Modulus=<uppercase hex>"; the leading byte of an RSA modulus
    # always has its top bit set, so there is no leading zero to strip.
    modulus_line = openssl(pem, "-modulus", "-noout").strip()
    modulus = bytes.fromhex(modulus_line.split("=", 1)[1])

    # Read the exponent rather than assuming 65537: a mismatch would only surface as
    # an opaque BCryptImportKeyPair failure at runtime.
    text = openssl(pem, "-text", "-noout")
    match = re.search(r"Exponent:\s*(\d+)", text)
    if not match:
        raise SystemExit("could not read the public exponent from the key")
    exponent = int(match.group(1))
    exponent_bytes = exponent.to_bytes((exponent.bit_length() + 7) // 8, "big")

    # Header fields are little-endian ULONGs; the exponent and modulus that follow
    # are big-endian byte strings. Prime lengths are zero for a public key.
    header = struct.pack("<6I", RSA_PUBLIC_MAGIC, len(modulus) * 8,
                         len(exponent_bytes), len(modulus), 0, 0)
    return header + exponent_bytes + modulus


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {sys.argv[0]} <public-key.pem> <output.bin>")
    pem, out = sys.argv[1], sys.argv[2]
    blob = build_blob(pem)
    with open(out, "wb") as f:
        f.write(blob)
    print(f"wrote {len(blob)} bytes to {out}")


if __name__ == "__main__":
    main()