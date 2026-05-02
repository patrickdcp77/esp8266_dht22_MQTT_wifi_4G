# Fiche d'information du projet

## Identite

- Nom du projet : ESP8266 D1 mini - DHT22 / MQTT / Wi-Fi
- Objectif : mesurer l'environnement avec un DHT22 et publier les donnees vers un broker MQTT
- Framework : Arduino
- Environnement de build : PlatformIO
- Carte cible : `d1_mini`

## Resume fonctionnel

Le programme initialise un DHT22 sur D2 (GPIO4), lit periodiquement ses mesures, affiche les resultats sur le port serie, puis publie un message JSON sur un topic MQTT.

Au demarrage, le module scanne les reseaux Wi-Fi visibles, tente d'abord une connexion sur le SSID principal configure, puis bascule sur le SSID de secours si le principal n'est pas detecte.

Le projet est adapte a un usage de station meteo embarquee ou de telemetrie environnementale vers Node-RED, Home Assistant ou tout autre consommateur MQTT.

## Fonctions implementees

### 1. Acquisition des mesures

- Lecture DHT22 : temperature et humidite
- Validation des lectures avec rejet des valeurs `NaN`
- Reutilisation de la derniere mesure valide si une lecture ponctuelle echoue

### 2. Connectivite reseau

- Connexion au Wi-Fi en mode station
- Scan des SSID disponibles avant connexion
- Priorite au reseau principal avec fallback automatique vers le reseau de secours
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
  "device": "esp8266-dht22",
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

- DHT22 DATA -> D2 (GPIO4)
- DHT22 VCC -> 3V3
- DHT22 GND -> GND
- Resistance de tirage 10 kOhm recommandee entre DATA et 3V3 si le module n'en integre pas

Des visuels de brochage sont disponibles dans le dossier `documents/images/`.

## Dependances

- `DHT sensor library`
- `PubSubClient`

## Points techniques notables

- Le buffer MQTT est augmente a 768 octets pour accepter le payload JSON complet
- La cible PlatformIO active est `d1_mini` sur plateforme `espressif8266`
- Le DHT22 fournit temperature et humidite uniquement, sans pression ni altitude
- La publication MQTT est effectuee toutes les 60 secondes

## Configuration locale requise

Les secrets ne sont pas versionnes. Renseigner localement :

- SSID Wi-Fi principal
- mot de passe Wi-Fi principal
- SSID Wi-Fi de secours
- mot de passe Wi-Fi de secours
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