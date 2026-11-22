#!/usr/bin/env python3
"""Construit une démo de l'interface web, utilisable sans module, dans build/demo/.

Les pages de data/ sont copiées telles quelles ; seul tools/demo/demo.js (un faux
module qui simule le poêle, la pièce et le mode autonome) est ajouté avant
common.js. Ouvrir build/demo/index.html dans un navigateur.

Usage : python3 tools/make_demo.py [--artifact] [dossier]   (depuis la racine du projet)

--artifact : index.html sans squelette <html>/<head>/<body> (pour une page hébergée
             qui ajoute le sien) et titre « Démo poêle MCZ ».
"""
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "data")
ARGS = [a for a in sys.argv[1:] if not a.startswith("--")]
ARTIFACT = "--artifact" in sys.argv
OUT = ARGS[0] if ARGS else os.path.join(ROOT, "build", "demo")
HOOK = '<script src="common.js"></script>'


def strip_skeleton(html):
    """Retire doctype, <html>, <head>, <body> en gardant leur contenu."""
    for tag in ("<!doctype html>", '<html lang="fr">', "<head>", "</head>", "<body>", "</body>", "</html>"):
        html = html.replace(tag + "\n", "").replace(tag, "")
    return html


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)
    for name in sorted(os.listdir(DATA)):
        src = os.path.join(DATA, name)
        dst = os.path.join(OUT, name)
        if name.endswith(".html"):
            with open(src, encoding="utf-8") as f:
                html = f.read()
            if HOOK not in html:
                sys.exit(f"{name} : {HOOK} introuvable")
            html = html.replace(HOOK, '<script src="demo.js"></script>\n' + HOOK)
            # pas de mise à jour du firmware dans la démo
            html = html.replace('href="/update"', 'href="#" onclick="toast(\'Pas de mise à jour dans la démo\');return false"')
            if ARTIFACT and name == "index.html":
                html = strip_skeleton(html).replace("<title>Poêle MCZ</title>", "<title>Démo poêle MCZ</title>")
            with open(dst, "w", encoding="utf-8") as f:
                f.write(html)
        elif os.path.isfile(src):
            shutil.copy(src, dst)
    shutil.copy(os.path.join(ROOT, "tools", "demo", "demo.js"), OUT)
    print(f"Démo générée dans {OUT} : ouvrez index.html dans un navigateur")


if __name__ == "__main__":
    main()
