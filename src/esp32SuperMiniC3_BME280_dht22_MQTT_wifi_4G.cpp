#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>

#include "config_secrets.h"

// =====================================================
// Broches ESP32-C3 Super Mini
// =====================================================

#define SDA_PIN 8
#define SCL_PIN 9

// =====================================================
// Capteurs
// =====================================================

#define BME280_ADDRESS 0x76
#define SEALEVEL_PRESSURE_HPA 1013.25f

#define DHT22_PIN 4
#define DHT_TYPE DHT22

// Publication toutes les 5 secondes
constexpr unsigned long PUBLISH_INTERVAL_MS = 5000UL;

namespace {

const char *flashedSketchName() {
  const char *fullPath = __FILE__;
  const char *lastSlash = strrchr(fullPath, '/');
  return lastSlash ? lastSlash + 1 : fullPath;
}

struct MeasurementFrame {
  bool bmeValid;
  float externalTemperatureC;
  float externalHumidity;
  float pressureHpa;
  float altitudeM;

  bool dhtValid;
  float internalTemperatureC;
  float internalHumidity;

  int wifiRssi;
};

Adafruit_BME280 bme;
DHT dht(DHT22_PIN, DHT_TYPE);

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool bmeReady = false;

// =====================================================
// Lecture des mesures
// =====================================================

MeasurementFrame readMeasurements() {
  MeasurementFrame frame{};

  frame.bmeValid = false;
  frame.dhtValid = false;

  frame.externalTemperatureC = NAN;
  frame.externalHumidity = NAN;
  frame.pressureHpa = NAN;
  frame.altitudeM = NAN;

  frame.internalTemperatureC = NAN;
  frame.internalHumidity = NAN;

  frame.wifiRssi = WiFi.RSSI();

  if (bmeReady) {
    frame.externalTemperatureC = bme.readTemperature();
    frame.externalHumidity = bme.readHumidity();
    frame.pressureHpa = bme.readPressure() / 100.0f;
    frame.altitudeM = bme.readAltitude(SEALEVEL_PRESSURE_HPA);

    frame.bmeValid =
      !isnan(frame.externalTemperatureC) &&
      !isnan(frame.externalHumidity) &&
      !isnan(frame.pressureHpa);
  }

  frame.internalTemperatureC = dht.readTemperature();
  frame.internalHumidity = dht.readHumidity();

  frame.dhtValid =
    !isnan(frame.internalTemperatureC) &&
    !isnan(frame.internalHumidity);

  return frame;
}

// =====================================================
// Connexion WiFi
// =====================================================

void ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Connexion Wi-Fi a ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

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
  } else {
    Serial.println("Echec de connexion Wi-Fi");
  }
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

  if (!ensureMqttConnected()) {
    Serial.println("Publication abandonnee : MQTT non connecte");
    return;
  }

  char payload[512];

  int written = snprintf(
    payload,
    sizeof(payload),
    "{"
      "\"device\":\"esp32c3-bme280-dht22\","
      "\"wifi_ssid\":\"%s\","
      "\"wifi_rssi\":%d,"
      "\"external\":{"
        "\"sensor\":\"BME280\","
        "\"address\":\"0x76\","
        "\"valid\":%s,"
        "\"temperature_c\":%.2f,"
        "\"humidity_pct\":%.2f,"
        "\"pressure_hpa\":%.2f,"
        "\"altitude_m\":%.2f"
      "},"
      "\"internal\":{"
        "\"sensor\":\"DHT22\","
        "\"pin\":%d,"
        "\"valid\":%s,"
        "\"temperature_c\":%.2f,"
        "\"humidity_pct\":%.2f"
      "}"
    "}",
    WIFI_SSID,
    frame.wifiRssi,
    frame.bmeValid ? "true" : "false",
    frame.externalTemperatureC,
    frame.externalHumidity,
    frame.pressureHpa,
    frame.altitudeM,
    DHT22_PIN,
    frame.dhtValid ? "true" : "false",
    frame.internalTemperatureC,
    frame.internalHumidity
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
  Serial.println("Mesures BME280 + DHT22");

  if (!frame.bmeValid) {
    Serial.println("Capteur BME280 non initialise ou lecture invalide");
  } else {
    Serial.println("BME280 (0x76)");

    Serial.print("Temperature : ");
    Serial.print(frame.externalTemperatureC, 2);
    Serial.println(" C");

    Serial.print("Humidite : ");
    Serial.print(frame.externalHumidity, 2);
    Serial.println(" %");

    Serial.print("Pression : ");
    Serial.print(frame.pressureHpa, 2);
    Serial.println(" hPa");

    Serial.print("Altitude approx. : ");
    Serial.print(frame.altitudeM, 2);
    Serial.println(" m");
  }

  Serial.println();

  Serial.print("DHT22 GPIO");
  Serial.println(DHT22_PIN);

  if (!frame.dhtValid) {
    Serial.println("Lecture DHT22 invalide");
  } else {
    Serial.print("Temperature : ");
    Serial.print(frame.internalTemperatureC, 2);
    Serial.println(" C");

    Serial.print("Humidite : ");
    Serial.print(frame.internalHumidity, 2);
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

  Wire.begin(SDA_PIN, SCL_PIN);
  delay(200);

  Serial.println();
  Serial.println("Demarrage ESP32-C3 meteo MQTT");

  Serial.print("I2C SDA = ");
  Serial.println(SDA_PIN);

  Serial.print("I2C SCL = ");
  Serial.println(SCL_PIN);

  Serial.print("Adresse BME280 = 0x");
  Serial.println(BME280_ADDRESS, HEX);

  Serial.print("DHT22 sur GPIO");
  Serial.println(DHT22_PIN);

  Serial.print("Broker MQTT : ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  Serial.print("Topic MQTT : ");
  Serial.println(MQTT_TOPIC);

  Serial.print("Client MQTT : ");
  Serial.println(MQTT_CLIENT_ID);

  bmeReady = bme.begin(BME280_ADDRESS, &Wire);

  if (!bmeReady) {
    Serial.println("Echec initialisation BME280 sur 0x76");
  } else {
    Serial.println("BME280 initialise");

    bme.setSampling(
      Adafruit_BME280::MODE_NORMAL,
      Adafruit_BME280::SAMPLING_X2,
      Adafruit_BME280::SAMPLING_X16,
      Adafruit_BME280::SAMPLING_X1,
      Adafruit_BME280::FILTER_X16,
      Adafruit_BME280::STANDBY_MS_500
    );
  }

  dht.begin();
  delay(2000);

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