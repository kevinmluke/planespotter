#!/usr/bin/env python3
"""Signs a firmware image and writes the OTA manifest.

  python3 tools/sign_firmware.py --url https://example.com/fw/firmware-1.1.0.bin
        [--bin .pio/build/planespotter/firmware.bin] [--out build/release]

Produces build/release/firmware-<version>.bin and manifest.json. Upload both to
the location OTA_MANIFEST_URL / --url point to.

The signature covers "planespotter-fw\\n<version>\\n<size>\\n<sha256 hex>\\n<url>\\n",
so a valid manifest cannot be replayed with another version number, size or file.
"""
import argparse
import base64
import hashlib
import json
import os
import shutil

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def signed_message(version, size, sha256_hex, url):
    return f"planespotter-fw\n{version}\n{size}\n{sha256_hex}\n{url}\n".encode()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", required=True, help="public URL the firmware .bin will be served from")
    ap.add_argument("--bin", default=os.path.join(ROOT, ".pio", "build", "planespotter", "firmware.bin"))
    ap.add_argument("--key", default=os.path.join(ROOT, "keys", "ota_private.pem"))
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "release"))
    ap.add_argument("--notes", default="")
    args = ap.parse_args()

    version = open(os.path.join(ROOT, "VERSION")).read().strip()
    data = open(args.bin, "rb").read()
    if data[0] != 0xE9:
        raise SystemExit("not an ESP32 app image (missing 0xE9 magic)")
    sha = hashlib.sha256(data).hexdigest()
    with open(args.key, "rb") as f:
        key = serialization.load_pem_private_key(f.read(), password=None)
    sig = key.sign(signed_message(version, len(data), sha, args.url), ec.ECDSA(hashes.SHA256()))

    os.makedirs(args.out, exist_ok=True)
    bin_name = f"firmware-{version}.bin"
    shutil.copyfile(args.bin, os.path.join(args.out, bin_name))
    manifest = {
        "version": version,
        "url": args.url,
        "size": len(data),
        "sha256": sha,
        "signature": base64.b64encode(sig).decode(),
        "notes": args.notes,
    }
    with open(os.path.join(args.out, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    # Self-check with the public key, exactly as the device will
    key.public_key().verify(sig, signed_message(version, len(data), sha, args.url), ec.ECDSA(hashes.SHA256()))
    print(f"signed {bin_name}: {len(data)} bytes, sha256 {sha[:16]}...")
    print("upload:", os.path.join(args.out, bin_name), "and", os.path.join(args.out, "manifest.json"))


if __name__ == "__main__":
    main()
