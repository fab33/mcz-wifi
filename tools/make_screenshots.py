#!/usr/bin/env python3
"""Régénère les captures d'écran de la documentation (docs/images/) à partir de la
démo (données fictives), avec Chrome/Chromium sans écran.

Usage : python3 tools/make_screenshots.py   (depuis la racine du projet)
"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "images")
DEMO = os.path.join(ROOT, "build", "shots")
# nom, largeur x hauteur (px CSS), page
SHOTS = [
    ("accueil", "420,745", "index.html"),
    ("programmation", "420,1140", "schedule.html?slot=0,1"),
    ("reglages-mode-autonome", "420,800", "settings.html?only=%23auto-hyst"),
]


def chrome():
    for name in ("google-chrome", "chromium", "chromium-browser"):
        path = shutil.which(name)
        if path:
            return path
    sys.exit("Chrome ou Chromium introuvable")


def main():
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "make_demo.py"), DEMO], check=True)
    shutil.copy(os.path.join(ROOT, "tools", "demo", "shot.js"), DEMO)
    for page in ("index", "schedule", "settings"):
        path = os.path.join(DEMO, page + ".html")
        with open(path, encoding="utf-8") as f:
            html = f.read()
        tag = f'<script src="{page}.js"></script>'
        with open(path, "w", encoding="utf-8") as f:
            f.write(html.replace(tag, tag + '<script src="shot.js"></script>'))
    os.makedirs(OUT, exist_ok=True)
    for name, size, page in SHOTS:
        with tempfile.TemporaryDirectory() as profile:
            subprocess.run([chrome(), "--headless=new", "--disable-gpu", "--no-sandbox", "--hide-scrollbars",
                            "--force-device-scale-factor=2", f"--user-data-dir={profile}",
                            "--virtual-time-budget=6000", f"--window-size={size}",
                            f"--screenshot={os.path.join(OUT, name + '.png')}", f"file://{DEMO}/{page}"],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=90)
        print(f"docs/images/{name}.png")


if __name__ == "__main__":
    main()
