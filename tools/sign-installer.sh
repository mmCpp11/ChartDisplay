#!/bin/sh
# Sign the release installer for ChartDisplay's self-updater, then verify the result.
#
#   ./sign-installer.sh /path/to/InstallChartDisplay.exe
#
# Produces <installer>.sig beside the installer. Upload BOTH to the GitHub release,
# named exactly InstallChartDisplay.exe and InstallChartDisplay.exe.sig - the updater
# rebuilds those URLs from the tag and will not find them under any other name.
#
# The signature covers the installer's exact bytes, so re-run this after every rebuild.
#
# Algorithm is fixed by the verifier in ChartDisplay/Downloader.ixx: RSA PKCS#1 v1.5
# over a SHA-256 digest (BCRYPT_PKCS1_PADDING_INFO{BCRYPT_SHA256_ALGORITHM} with
# BCRYPT_PAD_PKCS1), which is what "openssl dgst -sign" emits by default. Do not
# switch to PSS without changing the verifier first.
#
# Do NOT generate a new key pair. Verification uses the public key compiled into the
# ALREADY INSTALLED build, so signing with a different key silently breaks self-update
# for every existing user. Rotating means shipping the new public key in a release
# signed with the OLD key, letting users update onto it, and only then switching.

set -eu

KEY_DIR="${CHARTDISPLAY_KEY_DIR:-$HOME/chartdisplay-signing}"
KEY="$KEY_DIR/signing-key.pem"
PUB="$KEY_DIR/signing-key.pub.pem"

INSTALLER="${1:-}"
if [ -z "$INSTALLER" ]; then
    echo "usage: $0 <path-to-InstallChartDisplay.exe>" >&2
    exit 2
fi
if [ ! -f "$INSTALLER" ]; then
    echo "error: no such file: $INSTALLER" >&2
    exit 1
fi
# The key must live on the Linux filesystem: under /mnt/ DrvFs makes the 0600 mode
# meaningless, so the private key would be world-readable from Windows.
case "$KEY_DIR" in
    /mnt/*) echo "error: key dir is under /mnt/, where Unix permissions do not apply: $KEY_DIR" >&2; exit 1 ;;
esac
if [ ! -f "$KEY" ]; then
    echo "error: private key not found: $KEY" >&2
    exit 1
fi

SIG="$INSTALLER.sig"

openssl dgst -sha256 -sign "$KEY" -out "$SIG" "$INSTALLER"

# Verified immediately: this is what catches signing a stale build or the wrong file,
# which otherwise only shows up as a failed update after the release is public.
if ! openssl dgst -sha256 -verify "$PUB" -signature "$SIG" "$INSTALLER"; then
    echo "error: signature did not verify, removing $SIG" >&2
    rm -f "$SIG"
    exit 1
fi

echo "signed: $SIG"
echo "sha256: $(sha256sum "$INSTALLER" | cut -d' ' -f1)"