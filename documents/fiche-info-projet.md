# Fiche d'information du projet

## Identite

- Nom du projet : ESP32 Super Mini C3 - DHT22 / MQTT / Wi-Fi
- Objectif : mesurer l'environnement avec un DHT22 et publier les donnees vers un broker MQTT
- Framework : Arduino
- Environnement de build : PlatformIO
- Carte cible : `esp32-c3-devkitc-02`

## Resume fonctionnel

Le programme initialise un DHT22 sur GPIO4, lit periodiquement ses mesures, affiche les resultats sur le port serie, puis publie un message JSON sur un topic MQTT.

Le projet est adapte a un usage de station meteo embarquee ou de telemetrie environnementale vers Node-RED, Home Assistant ou tout autre consommateur MQTT.

## Fonctions implementees

### 1. Acquisition des mesures

- Lecture DHT22 : temperature et humidite
- Validation des lectures avec rejet des valeurs `NaN`

### 2. Connectivite reseau

- Connexion au Wi-Fi en mode station
- Desactivation de la mise en veille Wi-Fi pour une liaison plus stable
- Affichage de l'adresse IP locale et du RSSI

### 3. Publication MQTT

- Resolution DNS du broker pour diagnostic
- Test TCP direct avant connexion MQTT
- Connexion authentifiee au broker
- Publication d'un payload JSON retenu (`retain=true`)
- Reconnexion automatique si la liaison MQTT est perdue

### 4. Diagnostic serie

- Affichage du nom du fichier flashe
- Etat d'initialisation du capteur
- Affichage detaille des mesures
- Traces de debug lors des echecs Wi-Fi, DNS, TCP ou MQTT

## Structure des donnees publiees

Le payload JSON contient :

- l'identite de l'appareil
- le SSID Wi-Fi et le RSSI
- un bloc `measurement` pour le DHT22

Exemple simplifie :

```json
{
  "device": "esp32c3-dht22",
  "wifi_ssid": "mon-reseau",
  "wifi_rssi": -61,
  "measurement": {
    "sensor": "DHT22",
    "gpio": 4,
    "valid": true,
    "temperature_c": 24.13,
    "humidity_pct": 48.20
  }
}
```

## Cablage actuel

- DHT22 DATA -> GPIO4
- DHT22 VCC -> 3V3
- DHT22 GND -> GND
- Resistance de tirage 10 kOhm recommandee entre DATA et 3V3 si le module n'en integre pas

Des visuels de brochage sont disponibles dans le dossier `documents/images/`.

## Dependances

- `DHT sensor library`
- `PubSubClient`

## Points techniques notables

- Le buffer MQTT est augmente a 768 octets pour accepter le payload JSON complet
- Les drapeaux `ARDUINO_USB_MODE=1` et `ARDUINO_USB_CDC_ON_BOOT=1` sont actifs pour le port serie USB natif de l'ESP32-C3
- Le DHT22 fournit temperature et humidite uniquement, sans pression ni altitude

## Configuration locale requise

Les secrets ne sont pas versionnes. Renseigner localement :

- SSID Wi-Fi
- mot de passe Wi-Fi
- hote MQTT
- port MQTT
- topic MQTT
- identifiant client MQTT
- utilisateur MQTT
- mot de passe MQTT

Le modele de fichier est fourni dans [include/config_secrets.h.example](../include/config_secrets.h.example).

## Lancement

1. Creer `include/config_secrets.h` a partir du modele.
2. Compiler et flasher avec PlatformIO.
3. Ouvrir le moniteur serie a `115200` bauds.
4. Verifier la connexion Wi-Fi, puis MQTT.
5. Controler la reception des messages sur le topic configure.