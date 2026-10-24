#!/usr/bin/env python3
"""Builds, signs and publishes a firmware release to GitHub.

  1. bump the number in VERSION (e.g. 1.0.0 -> 1.0.1) and commit
  2. python3 tools/release.py --notes "What changed"
     (runs the host tests, refuses uncommitted changes, publishes a GitHub pre-release)
  3. try it on your own display, then: python3 tools/release.py --promote

The displays check https://github.com/<GITHUB_REPO>/releases/latest/download/manifest.json
once a day. A pre-release is never "latest", so only --promote updates every unit.
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
    ap.add_argument("--promote", action="store_true",
                    help="mark the already published pre-release for VERSION as latest (ships it to every display)")
    args = ap.parse_args()

    version = open(os.path.join(ROOT, "VERSION")).read().strip()
    cfg = open(os.path.join(ROOT, "include", "config.h")).read()
    repo = re.search(r'#define GITHUB_REPO "([^"]+)"', cfg).group(1)
    if repo.startswith("YOUR-"):
        sys.exit("Set GITHUB_REPO in include/config.h first (e.g. \"jane/planespotter\").")
    tag = f"v{version}"
    url = f"https://github.com/{repo}/releases/download/{tag}/firmware-{version}.bin"
    gh = shutil.which("gh")

    if args.promote:
        if not gh:
            sys.exit(f"GitHub CLI not found. On https://github.com/{repo}/releases edit {tag}: "
                     "untick 'Set as a pre-release' and tick 'Set as the latest release'.")
        subprocess.run([gh, "release", "edit", tag, "--repo", repo, "--prerelease=false", "--latest"], check=True)
        print(f"\nPromoted {tag} to latest. Displays will pick it up within 24 hours.")
        return

    dirty = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True, check=True).stdout
    if dirty.strip():
        sys.exit("Uncommitted changes; commit or stash them first so the release matches the source:\n" + dirty)
    subprocess.run(["make", "-C", os.path.join(ROOT, "test", "host"), "test"], check=True)

    pio = shutil.which("pio") or os.path.expanduser("~/.platformio/penv/bin/pio")
    if not args.skip_build:
        subprocess.run([pio, "run"], cwd=ROOT, check=True)
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "sign_firmware.py"), "--url", url, "--notes", args.notes],
                   cwd=ROOT, check=True)
    out = os.path.join(ROOT, "build", "release")
    files = [os.path.join(out, f"firmware-{version}.bin"), os.path.join(out, "manifest.json")]

    if not gh:
        print(f"\nGitHub CLI not found. Create release {tag} on https://github.com/{repo}/releases/new,")
        print("tick 'Set as a pre-release' and attach these two files (names must stay as they are):")
        for f in files:
            print("  ", f)
        return
    subprocess.run([gh, "release", "create", tag, *files, "--repo", repo, "--title", f"PlaneSpotter {version}",
                    "--notes", args.notes or f"PlaneSpotter {version}", "--prerelease"], check=True)
    print(f"\nPublished {tag} as a pre-release: no display will install it yet.")
    print("Flash or test it on your own unit, then run: python3 tools/release.py --promote")


if __name__ == "__main__":
    main()
