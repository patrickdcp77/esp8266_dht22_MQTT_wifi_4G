# Fiche d'information du projet

## Identite

- Nom du projet : ESP32 Super Mini C3 - BME280 / MQTT / Wi-Fi
- Objectif : mesurer l'environnement avec un BME280 et publier les donnees vers un broker MQTT
- Framework : Arduino
- Environnement de build : PlatformIO
- Carte cible : `esp32-c3-devkitc-02`

## Resume fonctionnel

Le programme initialise un BME280 en I2C, lit periodiquement ses mesures, affiche les resultats sur le port serie, puis publie un message JSON sur un topic MQTT.

Le projet est adapte a un usage de station meteo embarquee ou de telemetrie environnementale vers Node-RED, Home Assistant ou tout autre consommateur MQTT.

## Fonctions implementees

### 1. Acquisition des mesures

- Lecture BME280 : temperature, humidite, pression, altitude estimee
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
- un bloc `measurement` pour le BME280

Exemple simplifie :

```json
{
  "device": "esp32c3-bme280",
  "wifi_ssid": "mon-reseau",
  "wifi_rssi": -61,
  "measurement": {
    "sensor": "BME280",
    "address": "0x76",
    "valid": true,
    "temperature_c": 24.13,
    "humidity_pct": 48.20,
    "pressure_hpa": 1012.44,
    "altitude_m": 6.82
  }
}
```

## Cablage actuel

- BME280 SDA -> GPIO8
- BME280 SCL -> GPIO9
- VCC/GND selon la tension supportee par les modules utilises

Des visuels de brochage sont disponibles dans le dossier `documents/images/`.

## Dependances

- `Adafruit BME280 Library`
- `PubSubClient`

## Points techniques notables

- Le buffer MQTT est augmente a 768 octets pour accepter le payload JSON complet
- Les drapeaux `ARDUINO_USB_MODE=1` et `ARDUINO_USB_CDC_ON_BOOT=1` sont actifs pour le port serie USB natif de l'ESP32-C3
- Le niveau de la mer est fixe a `1013.25 hPa` pour le calcul d'altitude

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