#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GPS.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>  // Include the Adafruit MPU6050 library
#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

// Define the SSID and password for your WiFi
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASS       "your_wifi_password"

// Adafruit MQTT configuration
#define MQTT_SERVER     "io.adafruit.com"
#define MQTT_PORT       1883
#define MQTT_USERNAME   "your_adafruit_username"
#define MQTT_KEY        "your_adafruit_key"

// Create an Adafruit GPS object
Adafruit_GPS GPS(&Wire);

// Create an Adafruit MPU6050 object
Adafruit_MPU6050 mpu;

// Create WiFi and MQTT clients
WiFiClient wifiClient;
Adafruit_MQTT_Client mqttClient(&wifiClient, MQTT_SERVER, MQTT_PORT, MQTT_USERNAME, MQTT_KEY);

// Adafruit MQTT topics
Adafruit_MQTT_Publish gpsFeed = Adafruit_MQTT_Publish(&mqttClient, MQTT_USERNAME "/feeds/gps");
Adafruit_MQTT_Publish speedFeed = Adafruit_MQTT_Publish(&mqttClient, MQTT_USERNAME "/feeds/speed");
Adafruit_MQTT_Publish accelFeed = Adafruit_MQTT_Publish(&mqttClient, MQTT_USERNAME "/feeds/acceleration");

// Complementary filter variables
const float alpha = 0.98;  // Weight for accelerometer data
float pitch, roll;

void setup() {
  Serial.begin(115200);
  delay(10);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Start the GPS module
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  // Start the MPU6050 sensor
  if (!mpu.begin()) {
    Serial.println("Could not find a valid MPU6050 sensor, check wiring!");
    while (1);
  }
  Serial.println("MPU6050 sensor initialized");

  Serial.println("Setup complete");
}

void loop() {
  // Read GPS data
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA())) {
      return;
    }
  }

  // Check if GPS fix is valid
  if (GPS.fix) {
    // Publish GPS data to Adafruit MQTT
    String gpsData = String(GPS.latitudeDegrees, 6) + "," + String(GPS.longitudeDegrees, 6);
    gpsFeed.publish(gpsData.c_str());
    Serial.println("Published GPS data: " + gpsData);

    // Calculate and publish speed (hypothetical formula, adjust as needed)
    float speed = GPS.speed * 1.852; // Convert from knots to km/h
    speedFeed.publish(String(speed).c_str());
    Serial.println("Published speed: " + String(speed) + " km/h");
  }

  // Read MPU6050 sensor data
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Apply complementary filter for pitch and roll
  pitch = alpha * (pitch + g.gyro.y * 0.01) + (1 - alpha) * a.acceleration.x;
  roll = alpha * (roll + g.gyro.x * 0.01) + (1 - alpha) * a.acceleration.y;

  // Publish filtered accelerometer data to Adafruit MQTT
  String accelData = String(pitch) + "," + String(roll) + "," + String(a.acceleration.z);
  accelFeed.publish(accelData.c_str());
  Serial.println("Published filtered accelerometer data: " + accelData);

  // Ensure the MQTT client is connected
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  // Maintain MQTT connection
  mqttClient.loop();
}

void connectMQTT() {
  Serial.println("Connecting to MQTT...");
  while (!mqttClient.connected()) {
    if (mqttClient.connect("ESP32Client")) {
      Serial.println("Connected to MQTT");
    } else {
      Serial.println("Failed to connect to MQTT, retrying in 5 seconds...");
      delay(5000);
    }
  }
}
