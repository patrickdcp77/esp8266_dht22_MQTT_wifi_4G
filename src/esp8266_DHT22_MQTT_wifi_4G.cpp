#include <Arduino.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <math.h>

#include "config_secrets.h"

// =====================================================
// Broches ESP8266 D1 mini
// =====================================================

#define DHT_PIN D2
#define DHT_TYPE DHT22

// =====================================================
// Capteurs
// =====================================================

// Publication toutes les 1mn
constexpr unsigned long PUBLISH_INTERVAL_MS = 60000UL;
constexpr uint8_t DHT_READ_RETRY_COUNT = 3;
constexpr unsigned long DHT_RETRY_DELAY_MS = 250UL;

namespace {

const char *flashedSketchName() {
  const char *fullPath = __FILE__;
  const char *lastSlash = strrchr(fullPath, '/');
  return lastSlash ? lastSlash + 1 : fullPath;
}

struct MeasurementFrame {
  bool sensorValid;
  bool usedCachedValues;
  float temperatureC;
  float humidity;

  int wifiRssi;
};

DHT dht(DHT_PIN, DHT_TYPE);

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

float lastValidTemperatureC = NAN;
float lastValidHumidity = NAN;

// =====================================================
// Lecture des mesures
// =====================================================

MeasurementFrame readMeasurements() {
  MeasurementFrame frame{};

  frame.sensorValid = false;
  frame.usedCachedValues = false;

  frame.temperatureC = NAN;
  frame.humidity = NAN;

  frame.wifiRssi = WiFi.RSSI();

  for (uint8_t attempt = 0; attempt < DHT_READ_RETRY_COUNT; ++attempt) {
    float temperatureC = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (!isnan(temperatureC) && !isnan(humidity)) {
      frame.temperatureC = temperatureC;
      frame.humidity = humidity;
      frame.sensorValid = true;

      lastValidTemperatureC = temperatureC;
      lastValidHumidity = humidity;
      break;
    }

    if (attempt + 1 < DHT_READ_RETRY_COUNT) {
      delay(DHT_RETRY_DELAY_MS);
    }
  }

  if (!frame.sensorValid &&
      !isnan(lastValidTemperatureC) &&
      !isnan(lastValidHumidity)) {
    frame.temperatureC = lastValidTemperatureC;
    frame.humidity = lastValidHumidity;
    frame.sensorValid = true;
    frame.usedCachedValues = true;
  }

  return frame;
}

// =====================================================
// Connexion WiFi
// =====================================================

bool connectToWifi(const char* ssid, const char* password) {
  Serial.print("Connexion Wi-Fi a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connecte, IP : ");
    Serial.println(WiFi.localIP());

    Serial.print("RSSI Wi-Fi : ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    return true;
  }

  Serial.println("Echec de connexion Wi-Fi");
  return false;
}

void ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.disconnect();
  delay(100);

  const char* selectedSsid = WIFI_SSID;
  const char* selectedPassword = WIFI_PASSWORD;
  bool primaryFound = false;
  bool fallbackFound = false;

  Serial.println("Scan des reseaux Wi-Fi...");
  int networkCount = WiFi.scanNetworks();

  if (networkCount >= 0) {
    for (int i = 0; i < networkCount; ++i) {
      String detectedSsid = WiFi.SSID(i);

      if (detectedSsid == WIFI_SSID) {
        primaryFound = true;
      }

      if (detectedSsid == WIFI_SSID_FALLBACK) {
        fallbackFound = true;
      }
    }

    WiFi.scanDelete();

    if (!primaryFound && fallbackFound) {
      selectedSsid = WIFI_SSID_FALLBACK;
      selectedPassword = WIFI_PASSWORD_FALLBACK;
      Serial.println("Reseau principal introuvable, bascule sur le Wi-Fi de secours.");
    } else if (!primaryFound) {
      Serial.println("Aucun des SSID configures n'a ete detecte, tentative sur le Wi-Fi principal.");
    }
  } else {
    Serial.println("Scan Wi-Fi impossible, tentative sur le Wi-Fi principal.");
  }

  connectToWifi(selectedSsid, selectedPassword);
}

// =====================================================
// Connexion MQTT
// =====================================================

bool ensureMqttConnected() {
  if (mqttClient.connected()) {
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("MQTT impossible : Wi-Fi non connecte");
    return false;
  }

  Serial.print("Connexion MQTT a ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  // Diagnostic DNS
  IPAddress brokerIp;
  if (WiFi.hostByName(MQTT_HOST, brokerIp)) {
    Serial.print("IP broker resolue : ");
    Serial.println(brokerIp);
  } else {
    Serial.println("Erreur resolution DNS broker");
  }

  // Diagnostic TCP simple
  WiFiClient testClient;
  Serial.print("Test TCP vers broker : ");

  if (testClient.connect(MQTT_HOST, MQTT_PORT)) {
    Serial.println("OK");
    testClient.stop();
  } else {
    Serial.println("ECHEC");
    return false;
  }

  bool ok = mqttClient.connect(
    MQTT_CLIENT_ID,
    MQTT_USERNAME,
    MQTT_PASSWORD
  );

  if (ok) {
    Serial.println("MQTT connecte");
    return true;
  }

  Serial.print("Echec MQTT, rc=");
  Serial.println(mqttClient.state());

  return false;
}

// =====================================================
// Publication MQTT
// =====================================================

void publishMeasurements(const MeasurementFrame &frame) {
  ensureWifiConnected();

  if (!frame.sensorValid) {
    Serial.println("Publication abandonnee : mesure DHT22 invalide");
    return;
  }

  if (!ensureMqttConnected()) {
    Serial.println("Publication abandonnee : MQTT non connecte");
    return;
  }

  char payload[512];
  const String connectedSsid = WiFi.SSID();

  int written = snprintf(
    payload,
    sizeof(payload),
    "{"
      "\"device\":\"esp8266-dht22\","
      "\"wifi_ssid\":\"%s\","
      "\"wifi_rssi\":%d,"
      "\"measurement\":{"
        "\"sensor\":\"DHT22\","
        "\"gpio\":%d,"
        "\"valid\":%s,"
        "\"temperature_c\":%.2f,"
        "\"humidity_pct\":%.2f"
      "}"
    "}",
    connectedSsid.c_str(),
    frame.wifiRssi,
    DHT_PIN,
    frame.sensorValid ? "true" : "false",
    frame.temperatureC,
    frame.humidity
  );

  if (written < 0) {
    Serial.println("Erreur creation payload JSON");
    return;
  }

  if (written >= (int)sizeof(payload)) {
    Serial.println("Payload JSON trop long, publication abandonnee");
    return;
  }

  Serial.println();
  Serial.println("Payload MQTT :");
  Serial.println(payload);

  Serial.print("Longueur payload MQTT = ");
  Serial.println(strlen(payload));

  Serial.print("Taille buffer MQTT = ");
  Serial.println(mqttClient.getBufferSize());

  bool ok = mqttClient.publish(MQTT_TOPIC, payload, true);

  if (ok) {
    Serial.print("MQTT publie sur ");
    Serial.println(MQTT_TOPIC);
  } else {
    Serial.println("Echec publication MQTT");

    Serial.print("Etat MQTT apres echec = ");
    Serial.println(mqttClient.state());

    Serial.print("Connecte MQTT ? ");
    Serial.println(mqttClient.connected() ? "oui" : "non");
  }
}

// =====================================================
// Affichage série + publication
// =====================================================

void printMeasurements() {
  MeasurementFrame frame = readMeasurements();

  Serial.println();
  Serial.println("Mesures DHT22");

  if (!frame.sensorValid) {
    Serial.println("Capteur DHT22 non initialise ou lecture invalide");
  } else {
    Serial.print("DHT22 (GPIO ");
    Serial.print(DHT_PIN);
    Serial.println(")");

    if (frame.usedCachedValues) {
      Serial.println("Lecture DHT22 en echec, derniere mesure valide reutilisee");
    }

    Serial.print("Temperature : ");
    Serial.print(frame.temperatureC, 2);
    Serial.println(" C");

    Serial.print("Humidite : ");
    Serial.print(frame.humidity, 2);
    Serial.println(" %");
  }

  Serial.println();

  publishMeasurements(frame);
}

}  // namespace

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  unsigned long start = millis();

  while (!Serial && millis() - start < 4000) {
    delay(10);
  }

  Serial.println();
  Serial.print("Fichier flashe : ");
  Serial.println(flashedSketchName());

  Serial.println();
  Serial.println("Demarrage ESP8266 D1 mini meteo MQTT");

  Serial.println("Broche DHT22 = D2 (GPIO4)");

  Serial.print("Broker MQTT : ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  Serial.print("Topic MQTT : ");
  Serial.println(MQTT_TOPIC);

  Serial.print("Client MQTT : ");
  Serial.println(MQTT_CLIENT_ID);

  dht.begin();
  delay(2500);
  Serial.println("DHT22 initialise sur D2/GPIO4");

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  // Correction importante :
  // le JSON est trop long pour le buffer par defaut de PubSubClient.
  mqttClient.setBufferSize(768);

  ensureWifiConnected();
  ensureMqttConnected();

  printMeasurements();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  static unsigned long lastScan = 0;

  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  if (millis() - lastScan >= PUBLISH_INTERVAL_MS) {
    lastScan = millis();
    printMeasurements();
  }
}