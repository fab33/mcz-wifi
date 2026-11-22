#!/usr/bin/env python3
"""Génère les fichiers de release dans dist/ :

  mcz-wifi-<version>-complet.bin   image complète (firmware + interface web) à flasher à
                                   l'adresse 0x0 sur un module neuf (efface les réglages)
  mcz-wifi-<version>-firmware.bin  firmware seul, pour la mise à jour par l'interface web
                                   (conserve les réglages)
  mcz-wifi-<version>-interface.bin interface web seule (image LittleFS), pour la partie
                                   « FileSystem » de la mise à jour (efface les réglages)

Usage : python3 tools/make_release.py   (depuis la racine du projet)
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, ".pio", "build", "d1_mini")
FS_START = 0x300000  # eagle.flash.4m1m.ld
FS_SIZE = 0xFA000
PRIVATE_FILES = ("config.json", "state.json", "schedule.json")


def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)


def version():
    with open(os.path.join(ROOT, "include", "config.h"), encoding="utf-8") as f:
        return re.search(r'FIRMWARE_VERSION "([^"]+)"', f.read()).group(1)


def main():
    ver = version()
    run("pio", "run", "-e", "d1_mini")
    # Après un "uploadfs", l'image du build contient les réglages du module
    # (mots de passe) : on la reconstruit toujours à partir de data/ seul.
    fs_image = os.path.join(BUILD, "littlefs.bin")
    if os.path.exists(fs_image):
        os.remove(fs_image)
    run("pio", "run", "-e", "d1_mini", "-t", "buildfs")

    for f in PRIVATE_FILES:
        if os.path.exists(os.path.join(ROOT, "data", f)):
            sys.exit("data/%s ne doit pas exister (données personnelles)" % f)

    mkfs = os.path.expanduser("~/.platformio/packages/tool-mklittlefs/mklittlefs")
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "fs")
        subprocess.run([mkfs, "-u", out, "-p", "256", "-b", "8192", "-s", str(FS_SIZE), fs_image],
                       check=True, stdout=subprocess.DEVNULL)
        found = sorted(os.listdir(out))
        if any(f in found for f in PRIVATE_FILES):
            sys.exit("L'image LittleFS contient des réglages personnels : abandon")
        print("Contenu de l'interface web :", ", ".join(found))

    with open(os.path.join(BUILD, "firmware.bin"), "rb") as f:
        firmware = f.read()
    with open(fs_image, "rb") as f:
        fs = f.read()
    if len(firmware) > FS_START or len(fs) != FS_SIZE:
        sys.exit("Tailles inattendues (firmware %d, fs %d)" % (len(firmware), len(fs)))

    dist = os.path.join(ROOT, "dist")
    shutil.rmtree(dist, ignore_errors=True)
    os.makedirs(dist)
    full = os.path.join(dist, "mcz-wifi-%s-complet.bin" % ver)
    with open(full, "wb") as f:
        f.write(firmware)
        f.write(b"\xff" * (FS_START - len(firmware)))
        f.write(fs)
    shutil.copy(os.path.join(BUILD, "firmware.bin"), os.path.join(dist, "mcz-wifi-%s-firmware.bin" % ver))
    shutil.copy(fs_image, os.path.join(dist, "mcz-wifi-%s-interface.bin" % ver))
    for name in sorted(os.listdir(dist)):
        print("dist/%s (%d octets)" % (name, os.path.getsize(os.path.join(dist, name))))


if __name__ == "__main__":
    main()
