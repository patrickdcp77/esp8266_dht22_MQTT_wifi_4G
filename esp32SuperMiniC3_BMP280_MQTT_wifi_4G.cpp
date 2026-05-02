#include <Arduino.h>
#include <Adafruit_BME280.h>
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

#define BME280_ADDRESS_PRIMARY 0x76
#define BME280_ADDRESS_SECONDARY 0x77
#define SEALEVEL_PRESSURE_HPA 1016.0f
#define MIN_VALID_PRESSURE_HPA 850.0f
#define MAX_VALID_PRESSURE_HPA 1100.0f

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
  float temperatureC;
  float humidity;
  float pressurePa;
  float pressureHpa;
  float pressureMmHg;
  float altitudeM;

  int wifiRssi;
};

Adafruit_BME280 bme;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool bmeReady = false;
uint8_t bmeAddress = BME280_ADDRESS_PRIMARY;
float lastValidPressureHpa = NAN;
float lastValidAltitudeM = NAN;

void scanI2cBus() {
  Serial.println("Scan I2C en cours...");

  bool foundDevice = false;

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);

    if (Wire.endTransmission() == 0) {
      Serial.print("Peripherique I2C detecte a 0x");
      if (address < 16) {
        Serial.print('0');
      }
      Serial.println(address, HEX);
      foundDevice = true;
    }
  }

  if (!foundDevice) {
    Serial.println("Aucun peripherique I2C detecte");
  }
}

bool tryInitializeBmeAtAddress(uint8_t address) {
  if (!bme.begin(address, &Wire)) {
    return false;
  }

  bmeAddress = address;
  return true;
}

bool isPressurePlausible(float pressureHpa) {
  return
    !isnan(pressureHpa) &&
    pressureHpa >= MIN_VALID_PRESSURE_HPA &&
    pressureHpa <= MAX_VALID_PRESSURE_HPA;
}

// =====================================================
// Lecture des mesures
// =====================================================

MeasurementFrame readMeasurements() {
  MeasurementFrame frame{};

  frame.bmeValid = false;

  frame.temperatureC = NAN;
  frame.humidity = NAN;
  frame.pressurePa = NAN;
  frame.pressureHpa = NAN;
  frame.pressureMmHg = NAN;
  frame.altitudeM = NAN;

  frame.wifiRssi = WiFi.RSSI();

  if (bmeReady) {
    frame.temperatureC = bme.readTemperature();
    frame.humidity = bme.readHumidity();
    frame.pressurePa = bme.readPressure();
    frame.pressureHpa = frame.pressurePa / 100.0f;
    frame.pressureMmHg = frame.pressurePa / 133.322f;
    frame.altitudeM = bme.readAltitude(SEALEVEL_PRESSURE_HPA);

    if (isPressurePlausible(frame.pressureHpa)) {
      lastValidPressureHpa = frame.pressureHpa;
      lastValidAltitudeM = frame.altitudeM;
    } else if (!isnan(lastValidPressureHpa)) {
      Serial.print("Lecture pression aberrante ignoree : ");
      Serial.print(frame.pressureHpa, 2);
      Serial.println(" hPa");

      frame.pressureHpa = lastValidPressureHpa;
      frame.pressurePa = lastValidPressureHpa * 100.0f;
      frame.pressureMmHg = frame.pressurePa / 133.322f;
      frame.altitudeM = lastValidAltitudeM;
    } else {
      Serial.print("Lecture pression hors plage : ");
      Serial.print(frame.pressureHpa, 2);
      Serial.println(" hPa");

      frame.pressurePa = NAN;
      frame.pressureHpa = NAN;
      frame.pressureMmHg = NAN;
      frame.altitudeM = NAN;
    }

    frame.bmeValid =
      !isnan(frame.temperatureC) &&
      !isnan(frame.humidity) &&
      !isnan(frame.pressureHpa);
  }

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

  if (!frame.bmeValid) {
    Serial.println("Publication abandonnee : mesure BME280 invalide");
    return;
  }

  if (!ensureMqttConnected()) {
    Serial.println("Publication abandonnee : MQTT non connecte");
    return;
  }

  char payload[512];

  int written = snprintf(
    payload,
    sizeof(payload),
    "{"
      "\"device\":\"esp32c3-bme280\","
      "\"wifi_ssid\":\"%s\","
      "\"wifi_rssi\":%d,"
      "\"measurement\":{"
        "\"sensor\":\"BME280\","
        "\"address\":\"0x76\","
        "\"valid\":%s,"
        "\"temperature_c\":%.2f,"
        "\"humidity_pct\":%.2f,"
        "\"pressure_hpa\":%.2f,"
        "\"altitude_m\":%.2f"
      "}"
    "}",
    WIFI_SSID,
    frame.wifiRssi,
    frame.bmeValid ? "true" : "false",
    frame.temperatureC,
    frame.humidity,
    frame.pressureHpa,
    frame.altitudeM
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
  Serial.println("Mesures BME280");

  if (!frame.bmeValid) {
    Serial.println("Capteur BME280 non initialise ou lecture invalide");
  } else {
    Serial.println("BME280 (0x76)");

    Serial.print("Temperature : ");
    Serial.print(frame.temperatureC, 2);
    Serial.println(" C");

    Serial.print("Humidite : ");
    Serial.print(frame.humidity, 2);
    Serial.println(" %");

    Serial.print("Pression : ");
    Serial.print(frame.pressureHpa, 2);
    Serial.println(" hPa");

    Serial.print("Pression brute : ");
    Serial.print(frame.pressurePa, 2);
    Serial.println(" Pa");

    Serial.print("Equivalent mmHg : ");
    Serial.print(frame.pressureMmHg, 2);
    Serial.println(" mmHg");

    Serial.print("Altitude approx. : ");
    Serial.print(frame.altitudeM, 2);
    Serial.println(" m");
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

  Serial.print("Adresse BME280 prioritaire = 0x");
  Serial.println(BME280_ADDRESS_PRIMARY, HEX);

  Serial.print("Adresse BME280 secondaire = 0x");
  Serial.println(BME280_ADDRESS_SECONDARY, HEX);

  Serial.print("Broker MQTT : ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  Serial.print("Topic MQTT : ");
  Serial.println(MQTT_TOPIC);

  Serial.print("Client MQTT : ");
  Serial.println(MQTT_CLIENT_ID);

  scanI2cBus();

  bmeReady =
    tryInitializeBmeAtAddress(BME280_ADDRESS_PRIMARY) ||
    tryInitializeBmeAtAddress(BME280_ADDRESS_SECONDARY);

  if (!bmeReady) {
    Serial.println("Echec initialisation BME280 sur 0x76/0x77");
  } else {
    Serial.println("BME280 initialise");

    Serial.print("ID capteur = 0x");
    Serial.println(bme.sensorID(), HEX);

    Serial.print("Adresse active = 0x");
    Serial.println(bmeAddress, HEX);

    bme.setSampling(
      Adafruit_BME280::MODE_NORMAL,
      Adafruit_BME280::SAMPLING_X2,
      Adafruit_BME280::SAMPLING_X16,
      Adafruit_BME280::SAMPLING_X1,
      Adafruit_BME280::FILTER_X16,
      Adafruit_BME280::STANDBY_MS_500
    );
  }

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