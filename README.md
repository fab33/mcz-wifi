# MCZ WiFi : télécommande WiFi pour poêle à granulés MCZ

Pilotez votre poêle MCZ depuis votre téléphone, votre ordinateur ou **Home Assistant**, sans
toucher au poêle : un petit module WiFi remplace la télécommande radio d'origine.

- Marche / arrêt, puissance de flamme (1 à 5), ventilation (1 à 5 ou auto)
- Page web intégrée, utilisable depuis n'importe quel navigateur
- Intégration automatique dans Home Assistant (MQTT)
- Portée radio supérieure à celle de la télécommande d'origine
- Mise à jour du module sans câble

> Testé sur un **MCZ Ego Air 2.0** avec télécommande radio **868 MHz**. D'autres modèles MCZ
> utilisant la même télécommande 868 MHz devraient fonctionner. Les anciens modèles à télécommande
> 433 MHz ne sont **pas** compatibles avec ce matériel.

---

## 1. Matériel nécessaire (environ 15 à 25 €)

| Élément | Remarque |
|---|---|
| **Wemos D1 mini** (ESP8266) | ou un clone compatible |
| **Shield WeMos RFM69** de ch2i | [schémas et infos](https://github.com/hallard/WeMos-RFM69) — voir ci-dessous pour s'en passer |
| **Module radio RFM69HW 868 MHz** | les modèles RFM69W / RFM69CW fonctionnent aussi (moins de portée) — bien prendre la version **868** |
| **Antenne** | un simple fil rigide de **8,2 cm** soudé sur la sortie antenne suffit |
| **Câble micro-USB et chargeur USB 5 V** | pour alimenter le module en permanence |

Il faut savoir faire quelques soudures simples (module radio sur le shield, connecteurs).

**Sans le shield**, on peut relier directement un module RFM69 à la Wemos avec des fils :

| RFM69 | Wemos D1 mini |
|---|---|
| 3.3V | 3V3 |
| GND | G |
| NSS | D0 |
| SCK | D5 |
| MISO | D6 |
| MOSI | D7 |

Le module radio est alimenté en **3,3 V uniquement** : ne jamais le relier au 5 V.

## 2. Installer le logiciel sur le module

Aucun logiciel à installer sur l'ordinateur : tout se fait depuis le navigateur **Chrome** ou
**Edge** (Firefox et Safari ne savent pas parler au port USB).

1. Téléchargez le fichier **`mcz-wifi-x.y.z-complet.bin`** depuis la
   [dernière version publiée](../../releases/latest).
2. Branchez la Wemos à l'ordinateur avec le câble USB.
   *Sous Windows, si rien n'est détecté, installez le pilote « CH340 » (puce USB de la Wemos).*
3. Ouvrez l'outil officiel d'Espressif : **https://espressif.github.io/esptool-js/**
4. Cliquez sur **Connect** et choisissez le port de la Wemos (souvent « USB Serial » ou « CH340 »).
5. Cliquez sur **Erase Flash** et attendez la fin (efface un éventuel ancien programme).
6. Dans la ligne d'adresse, laissez **`0x0`**, choisissez le fichier `…-complet.bin`, puis
   cliquez sur **Program**. Comptez une minute environ.
7. Débranchez puis rebranchez la Wemos.

## 3. Connecter le module à votre WiFi

1. Avec votre téléphone, connectez-vous au réseau WiFi **`MCZ-xxxxxx`**
   (mot de passe : **`mczconfig`**). La première LED du module clignote en bleu.
2. La page de configuration s'ouvre toute seule. Sinon, ouvrez **http://192.168.4.1**.
3. Cliquez sur **Rechercher les réseaux**, choisissez votre box, saisissez son mot de passe
   (l'icône en forme d'œil permet de vérifier ce que vous tapez), puis **Enregistrer**.
4. Le module rejoint votre réseau. Reconnectez votre téléphone à votre WiFi habituel, puis ouvrez
   **http://poele-mcz.local**. Si cette adresse ne marche pas (fréquent sous Android), utilisez
   l'adresse IP du module, visible dans l'interface de votre box (appareil « poele-mcz »).

Conseil : dans **Réglages › Sécurité**, définissez un mot de passe pour l'interface (utilisateur
`admin`), sinon n'importe qui sur votre réseau peut commander le poêle.

## 4. Appairer le poêle avec le module

Le poêle n'obéit qu'à la télécommande avec laquelle il a été appairé. Chaque module a son propre
identifiant (visible dans **Réglages › Poêle**). Deux possibilités :

**A. Appairer le poêle avec le module** (cas général)
1. Mettez le poêle en mode « appairage télécommande » en suivant la **notice de votre poêle**
   (chapitre sur la télécommande). Sur beaucoup de modèles, cela se fait avec une touche ou un
   interrupteur à l'arrière du poêle.
2. Pendant que le poêle attend, cliquez sur **Renvoyer la commande** sur la page d'accueil du
   module. Le poêle confirme par une série de bips.
3. La télécommande d'origine peut ne plus être reconnue ensuite : c'est normal.

**B. Reprendre l'identifiant de la télécommande d'origine** (si vous le connaissez, par exemple
lu avec un boîtier RFXCOM) : saisissez-le dans **Réglages › Poêle › Identifiant**. Aucun
appairage n'est alors nécessaire.

**Si le poêle ne réagit pas** : dans **Réglages › Radio**, vérifiez que le modèle de module
correspond au vôtre (inscription sur le module : « RFM69HW » ou « RFM69W »). Avec un mauvais
réglage, le module n'émet presque rien.

## 5. Utilisation

La page d'accueil du module permet d'allumer et d'éteindre le poêle, et de régler la puissance
et la ventilation. Chaque commande est envoyée immédiatement ; le poêle bipe à la réception.

**À savoir :**
- **Le poêle ne renvoie aucune information** : le module affiche le dernier ordre envoyé, pas
  l'état réel du poêle. Si quelqu'un utilise la télécommande d'origine, l'affichage peut être faux.
- **Retirez les piles de la télécommande d'origine** : elle renvoie régulièrement ses propres
  réglages et écraserait ceux du module.
- Le module renvoie son état toutes les 5 minutes, **sans bip**, comme le fait la télécommande
  d'origine (réglable dans **Réglages › Poêle**).
- Les modes Manuel / Auto / Eco de la télécommande n'existent pas ici : ils sont gérés par la
  télécommande elle-même et ne changent rien au poêle. Pour réguler sur la température, utilisez
  Home Assistant (thermostat + sonde de température).
- Après une coupure de courant, le module **n'envoie rien** au poêle : il ne risque pas de
  l'allumer ou de l'éteindre par surprise.

**Bouton du module** : appui court = marche / arrêt du poêle ; appui long (8 s, LEDs violettes)
= effacement du WiFi et retour au réseau `MCZ-xxxxxx` de configuration.

**LEDs du module**

| LED | Couleur | Signification |
|---|---|---|
| 1 | bleu clignotant | en attente de configuration WiFi (réseau `MCZ-xxxxxx`) |
| 1 | vert | tout fonctionne |
| 1 | orange | WiFi OK, mais Home Assistant (MQTT) injoignable |
| 1 | rouge | WiFi perdu, ou module radio non détecté |
| 2 | orange | poêle en marche |
| 2 | flash blanc | commande envoyée (flash rouge : échec radio) |

## 6. Home Assistant

1. Il faut l'intégration **MQTT** dans Home Assistant (par exemple avec le module complémentaire
   « Mosquitto broker »).
2. Dans **Réglages › MQTT / Home Assistant** du module : cochez **Activer MQTT**, saisissez
   l'adresse du serveur (celle de Home Assistant si vous utilisez Mosquitto) et, si besoin,
   l'utilisateur et le mot de passe MQTT. Enregistrez.
3. Le poêle apparaît automatiquement : **Paramètres › Appareils et services › MQTT › Poêle MCZ**.
   Aucune notification « nouvel appareil » ne s'affiche : allez directement le chercher là.

Entités créées : interrupteur **Poêle**, **Puissance** (1 à 5), **Ventilation** (1 à 5 / auto),
bouton **Renvoyer la commande**, capteur **Signal WiFi**.

## 7. Mettre à jour le module

Téléchargez **`mcz-wifi-x.y.z-firmware.bin`** depuis la [dernière version](../../releases/latest),
puis dans **Réglages › Maintenance › Mettre à jour le firmware**, choisissez ce fichier dans la
partie *Firmware*. Les réglages sont conservés.

N'utilisez pas le fichier `…-complet.bin` pour une mise à jour : il effacerait vos réglages.

## 8. Dépannage

| Problème | Solution |
|---|---|
| Le réseau `MCZ-xxxxxx` n'apparaît pas | Vérifiez l'alimentation USB ; refaites l'étape 2 |
| Mot de passe WiFi erroné | Le module rouvre `MCZ-xxxxxx` au bout de 20 s : recommencez l'étape 3 |
| LED 1 rouge fixe dès le démarrage | Module radio non détecté : vérifiez ses soudures et son orientation |
| Le poêle ne réagit pas | Modèle de module (Réglages › Radio), appairage (étape 4), rapprochez le module du poêle |
| Le poêle change de réglage tout seul | La télécommande d'origine a encore ses piles |
| `poele-mcz.local` introuvable | Utilisez l'adresse IP du module (voir votre box) |
| Mot de passe de l'interface oublié | Réinstallez le module (étape 2, avec **Erase Flash**) puis refaites la configuration |

## Avertissement

Projet personnel, sans lien avec MCZ. Il n'intervient pas sur les sécurités du poêle, qui restent
assurées par le poêle lui-même, mais vous l'utilisez sous votre seule responsabilité. Le module
émet dans la bande 868 MHz : ne dépassez pas 14 dBm (25 mW) dans **Réglages › Radio**.

## Pour les développeurs

Compilation, architecture du code, protocole radio, API REST et topics MQTT :
voir la [documentation technique](docs/TECHNIQUE.md).

## Licence

[GPL-3.0](LICENSE). Le shield WeMos RFM69 est une création de Charles-Henri Hallard (ch2i),
sous sa propre licence.
