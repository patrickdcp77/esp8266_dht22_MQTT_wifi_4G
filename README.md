## ESP32 Super Mini C3 - DHT22 / MQTT

Projet PlatformIO pour ESP32-C3 Super Mini qui lit un capteur DHT22 sur GPIO4, affiche les mesures sur le port serie et les publie en JSON via MQTT.

La fiche projet complete est disponible dans [documents/fiche-info-projet.md](documents/fiche-info-projet.md).

### Fonctions principales

- Lecture temperature et humidite via DHT22
- Connexion Wi-Fi automatique et tentative de reconnexion
- Connexion MQTT avec diagnostic DNS/TCP
- Publication periodique d'un payload JSON conserve sur le broker
- Trace serie pour le diagnostic local

### Materiel et configuration

- Carte cible : ESP32-C3 DevKitC-02 / Super Mini
- DHT22 : DATA GPIO4
- Vitesse moniteur serie : 115200 bauds

### Secrets de configuration

Les identifiants Wi-Fi et MQTT ne sont plus versionnes. Copier [include/config_secrets.h.example](include/config_secrets.h.example) vers `include/config_secrets.h`, puis renseigner les valeurs locales avant compilation.
