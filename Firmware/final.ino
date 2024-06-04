

#include <WiFi.h>
#include <FirebaseESP32.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <MQ135.h>
#include <LiquidCrystal_I2C.h>

#define WIFI_SSID "Dialog 4G 352"
#define WIFI_PASSWORD "789A6Ce1"
#define API_KEY "AIzaSyAZj1uiJro05yzZ5osEBdxCIFQdTKRhnS8"
#define DATABASE_URL "https://environmental-monitering-default-rtdb.firebaseio.com/"

// Define DHT sensor pin and type
#define DHTPIN 4
#define DHTTYPE DHT22

#define GREEN_LED_PIN 13
#define RED_LED_PIN 12
#define BUZZER_PIN 14

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Define MQ135 analog pin
#define MQ135_PIN 34

// Initialize MQ135 sensor
MQ135 mq135(MQ135_PIN);

// Define LDR analog pin
#define LDR_PIN 35

// Calibration values for LDR
const int LDR_VALUE_LOW = 50;   // Analog reading in the dark
const int LDR_VALUE_HIGH = 4000; // Analog reading in bright light
const int LIGHT_INTENSITY_LOW = 0;  // Lux in the dark
const int LIGHT_INTENSITY_HIGH = 10000; // Lux in bright light

// Initialize the LCD display
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD address to 0x27 for a 16 chars and 2 line display

void setup() {
  // Start serial communication
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to wifi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize the DHT sensor
  dht.begin();
  Serial.println("DHT22 sensor initialized.");

  // Initialize the MQ135 sensor
  Serial.println("MQ135 sensor initialized.");

  // Initialize the LCD display
  lcd.begin();
  lcd.backlight();
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 500 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // Read temperature and humidity from DHT22
    float temperature = dht.readTemperature()-13;
    float humidity = dht.readHumidity()-20;

    // Check if any reads failed and exit early
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT sensor!");
      lcd.clear();
      lcd.print("DHT Error!");
      delay(2000);
      return;
    }

    // Read air quality from MQ135
    float airQuality = mq135.getPPM()-50;

    // Read light intensity from LDR
    int ldrValue = analogRead(LDR_PIN);

    // Convert analog reading to light intensity level in lux using calibration data
    float lightIntensity = map(ldrValue, LDR_VALUE_LOW, LDR_VALUE_HIGH, LIGHT_INTENSITY_LOW, LIGHT_INTENSITY_HIGH);

    // Send sensor data to Firebase
    if (Firebase.setFloat(fbdo, "/sensor_data/temperature", temperature)) {
      Serial.println("Temperature data sent to Firebase");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    } else {
      Serial.println("Failed to send temperature data to Firebase");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    Firebase.setFloat(fbdo, "/sensor_data/humidity", humidity);
    Firebase.setFloat(fbdo, "/sensor_data/air_quality", airQuality);
    Firebase.setFloat(fbdo, "/sensor_data/light_intensity", lightIntensity);

    // Print the results to the LCD display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T: ");
    lcd.print(temperature, 1); // Show temperature with one decimal place
    lcd.print("C H: ");
    lcd.print(humidity, 1); // Show humidity with one decimal place
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("AQ:");
    lcd.print(airQuality, 1); // Show air quality with one decimal place
    lcd.print("IN:");
    lcd.print(lightIntensity, 1); // Show light intensity with one decimal place
    // Wait for a few seconds before the next loop

    if (airQuality > 1000) { // Threshold for bad air quality
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(RED_LED_PIN, HIGH);
      tone(BUZZER_PIN, 1000); // Sound buzzer at 1kHz
    } else {
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
      noTone(BUZZER_PIN);
    }
    delay(2000);
  }
}

