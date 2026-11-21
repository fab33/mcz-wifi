# Script PlatformIO : conserve les réglages du module lors de "pio run -t uploadfs".
#
# "uploadfs" remplace tout le système de fichiers LittleFS, y compris les
# réglages (WiFi, MQTT, mots de passe) et le dernier état du poêle. Avant
# l'envoi, ce script :
#   1. lit le système de fichiers actuel de la carte (liaison série) ;
#   2. en extrait config.json et state.json ;
#   3. reconstruit l'image avec le contenu de data/ + ces deux fichiers.
#
# Une copie de la dernière sauvegarde est gardée dans .pio/fs_backup/
# (contient les mots de passe en clair, dossier ignoré par git).
#
# Pour envoyer data/ seul (réglages effacés) : MCZ_FS_NO_BACKUP=1 pio run -t uploadfs

import os
import shutil

Import("env")  # noqa: F821  (fourni par PlatformIO)

KEEP_FILES = ("config.json", "state.json")


def fail(msg):
    print("\n*** Sauvegarde des réglages impossible : %s" % msg)
    print("*** Envoi annulé pour ne pas effacer la configuration du module.")
    print("*** Pour envoyer quand même (réglages perdus) : MCZ_FS_NO_BACKUP=1 pio run -t uploadfs\n")
    env.Exit(1)


def keep_settings(target, source, env):
    if os.environ.get("MCZ_FS_NO_BACKUP") == "1":
        print("MCZ_FS_NO_BACKUP=1 : les réglages du module vont être effacés.")
        return
    if env.subst("$UPLOAD_PROTOCOL") != "esptool":
        fail("sauvegarde possible uniquement par câble USB (pas en OTA)")

    if not env.subst("$UPLOAD_PORT"):
        env.AutodetectUploadPort()
    port = env.subst("$UPLOAD_PORT")
    if not port:
        fail("carte introuvable sur le port série")

    image = str(source[0])
    fs_start, fs_size = env["FS_START"], env["FS_END"] - env["FS_START"]
    backup_dir = os.path.join(env.subst("$PROJECT_DIR"), ".pio", "fs_backup")
    flash_dump = os.path.join(backup_dir, "flash_fs.bin")
    extracted = os.path.join(backup_dir, "extrait")
    staging = os.path.join(backup_dir, "image")
    shutil.rmtree(backup_dir, ignore_errors=True)
    os.makedirs(backup_dir)

    print("Lecture des réglages présents sur la carte (%s)..." % port)
    read_cmd = '"$PYTHONEXE" "%s" --chip esp8266 --port "%s" --baud $UPLOAD_SPEED read_flash 0x%X 0x%X "%s"' % (
        os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py"),
        port, fs_start, fs_size, flash_dump)
    if env.Execute(env.VerboseAction(read_cmd, "Lecture de la mémoire flash")):
        fail("lecture de la mémoire flash échouée")

    unpack_cmd = '"$MKFSTOOL" -u "%s" -p $FS_PAGE -b $FS_BLOCK -s %d "%s"' % (extracted, fs_size, flash_dump)
    found = []
    if env.Execute(env.VerboseAction(unpack_cmd, "Extraction du système de fichiers")) == 0:
        found = [f for f in KEEP_FILES if os.path.isfile(os.path.join(extracted, f))]
    if not found:
        print("Aucun réglage trouvé sur la carte (première installation ?) : envoi de data/ seul.")
        return

    shutil.copytree(env.subst("$PROJECT_DATA_DIR"), staging)
    for f in found:
        shutil.copy2(os.path.join(extracted, f), os.path.join(staging, f))
        shutil.copy2(os.path.join(extracted, f), os.path.join(backup_dir, f))
    shutil.rmtree(extracted, ignore_errors=True)

    build_cmd = '"$MKFSTOOL" -c "%s" -p $FS_PAGE -b $FS_BLOCK -s %d "%s"' % (staging, fs_size, image)
    if env.Execute(env.VerboseAction(build_cmd, "Reconstruction de l'image avec les réglages")):
        fail("reconstruction de l'image échouée")
    shutil.rmtree(staging, ignore_errors=True)
    os.remove(flash_dump)
    print("Réglages conservés : %s (copie dans %s)" % (", ".join(found), backup_dir))


env.AddPreAction("uploadfs", keep_settings)
