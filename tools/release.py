#!/usr/bin/env python3
"""Builds, signs and publishes a firmware release to GitHub.

  1. bump the number in VERSION (e.g. 1.0.0 -> 1.0.1)
  2. python3 tools/release.py --notes "What changed"

The displays check https://github.com/<GITHUB_REPO>/releases/latest/download/manifest.json
once a day, so publishing the release is all it takes to update every unit.
Needs the GitHub CLI (`gh auth login` once); without it, the files to upload by hand are listed.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--notes", default="", help="release notes shown on GitHub")
    ap.add_argument("--skip-build", action="store_true")
    args = ap.parse_args()

    version = open(os.path.join(ROOT, "VERSION")).read().strip()
    cfg = open(os.path.join(ROOT, "include", "config.h")).read()
    repo = re.search(r'#define GITHUB_REPO "([^"]+)"', cfg).group(1)
    if repo.startswith("YOUR-"):
        sys.exit("Set GITHUB_REPO in include/config.h first (e.g. \"jane/planespotter\").")
    tag = f"v{version}"
    url = f"https://github.com/{repo}/releases/download/{tag}/firmware-{version}.bin"

    pio = shutil.which("pio") or os.path.expanduser("~/.platformio/penv/bin/pio")
    if not args.skip_build:
        subprocess.run([pio, "run"], cwd=ROOT, check=True)
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "sign_firmware.py"), "--url", url, "--notes", args.notes],
                   cwd=ROOT, check=True)
    out = os.path.join(ROOT, "build", "release")
    files = [os.path.join(out, f"firmware-{version}.bin"), os.path.join(out, "manifest.json")]

    gh = shutil.which("gh")
    if not gh:
        print(f"\nGitHub CLI not found. Create release {tag} on https://github.com/{repo}/releases/new")
        print("and attach these two files (names must stay as they are):")
        for f in files:
            print("  ", f)
        return
    subprocess.run([gh, "release", "create", tag, *files, "--repo", repo, "--title", f"PlaneSpotter {version}",
                    "--notes", args.notes or f"PlaneSpotter {version}"], check=True)
    print(f"\nPublished {tag}. Displays will pick it up within 24 hours (or press 'Check for updates' in settings).")


if __name__ == "__main__":
    main()
