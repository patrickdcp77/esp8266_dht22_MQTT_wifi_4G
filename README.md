## ESP8266 D1 mini - DHT22 / MQTT

Projet PlatformIO pour ESP8266 D1 mini qui lit un capteur DHT22 sur D2 (GPIO4), affiche les mesures sur le port serie et les publie en JSON via MQTT.

La fiche projet complete est disponible dans [documents/fiche-info-projet.md](documents/fiche-info-projet.md).

### Fonctions principales

- Lecture temperature et humidite via DHT22
- Scan Wi-Fi puis connexion prioritaire a `TP-Link_72E4`
- Bascule automatique sur `Freebox-CBAAFC` si le reseau principal est absent
- Connexion MQTT avec diagnostic DNS/TCP
- Publication periodique toutes les 60 secondes d'un payload JSON conserve sur le broker
- Trace serie pour le diagnostic local

### Materiel et configuration

- Carte cible : Wemos D1 mini / ESP8266
- DHT22 : DATA sur D2 (GPIO4)
- Vitesse moniteur serie : 115200 bauds

### Secrets de configuration

Les identifiants Wi-Fi et MQTT ne sont pas versionnes. Copier [include/config_secrets.h.example](include/config_secrets.h.example) vers `include/config_secrets.h`, puis renseigner les valeurs locales avant compilation.

Le fichier de secrets contient maintenant :

- un SSID principal et son mot de passe
- un SSID de secours et son mot de passe
- les parametres MQTT
