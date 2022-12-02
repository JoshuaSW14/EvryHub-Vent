// Software Capstone - EvryHub Vent
//
// @author  Joshua Symons-Webb
// @id      000812836
//
// I, Joshua Symons-Webb, 000812836 certify that this material is my original work. No
// other person's work has been used without due acknowledgement.

#include <Arduino.h>
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Stepper.h>
#include "DHT.h"

// AWS & WiFi Config
#define AWS_IOT_PUBLISH_TOPIC "evryhub/vent"
#define AWS_IOT_SUBSCRIBE_TOPIC "evryhub/vent/sub"
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// LED Pin (digital)
#define LED_PIN 21

// DHT Sensor
#define DHTPIN 33
#define DHTTYPE DHT22

// Air Quality Sensor
#define MQ_PIN 32

// Button Pins (digital)
#define openButtonPin 23
#define closeButtonPin 22

// Stepper Configuration (digital)
int stepsPerRevolution = 300;
Stepper myStepper(stepsPerRevolution, 17, 18, 5, 19); // IN1, IN3, IN2, IN4

// DHT
DHT dht(DHTPIN, DHTTYPE);
float humidity;
float temperature;
int desiredTemperature = 0;
int mqValue;

// ***********************************************************
void openVent(){
  myStepper.step(stepsPerRevolution);
  delay(2000);
}

// ***********************************************************
void closeVent(){
  myStepper.step(-stepsPerRevolution);
  delay(2000);
}

// ***********************************************************
void configVent(String value){
  desiredTemperature = value.toInt();
  delay(500);
}

// ***********************************************************
void messageHandler(char *topic, byte *payload, unsigned int length)
{
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);

  const char *device = doc["device"];
  const char *action = doc["action"];
  const char *value = doc["value"];

  Serial.printf("\n Device: %s | Action: %s | Value: %s\n", String(device), String(action), String(value));

  if (String(device) == "vent" && String(action) == "config")
  {
    Serial.println("Configure EvryHub Vent");
    configVent(String(value));
  }
  else if (String(device) == "vent" && String(action) == "open")
  {
    Serial.println("Open Vent");
    openVent();
  }
  else if (String(device) == "vent" && String(action) == "close")
  {
    Serial.println("Close Vent");
    closeVent();
  }
}

// ***********************************************************
void connectAWS()
{
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);

  // Create a message handler
  client.setCallback(messageHandler);
  Serial.println("Connecting to AWS IOT");

  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }

  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }
  digitalWrite(LED_PIN, LOW);

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");
}

// ***********************************************************
void publishMessage()
{
  StaticJsonDocument<200> doc;
  char jsonBuffer[512];
  if (isnan(humidity) || isnan(temperature))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
  }
  else
  {
    StaticJsonDocument<200> doc;
    doc["device"] = "vent";
    doc["action"] = "temperature";
    doc["value"] = String(temperature);
    serializeJson(doc, jsonBuffer);
    client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);

    doc["device"] = "vent";
    doc["action"] = "humidity";
    doc["value"] = String(humidity);
    serializeJson(doc, jsonBuffer);
    client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
  }

  doc["device"] = "vent";
  doc["action"] = "mq";
  doc["value"] = String(mqValue);
  serializeJson(doc, jsonBuffer);
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

// ***********************************************************
void wifiSetup()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    Serial.print(".");
  }
  digitalWrite(LED_PIN, HIGH);
}

// ***********************************************************
void otaSetup()
{
  ArduinoOTA
      .onStart([]()
               {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ***********************************************************
void checkInput()
{
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  mqValue = analogRead(MQ_PIN);

  while (digitalRead(openButtonPin) == LOW)
  {
    myStepper.step(stepsPerRevolution);
  }

  while (digitalRead(closeButtonPin) == LOW)
  {
    myStepper.step(-stepsPerRevolution);
  }
}

// ***********************************************************
void setup()
{
  myStepper.setSpeed(60);
  Serial.begin(115200);

  pinMode(openButtonPin, INPUT_PULLUP);
  pinMode(closeButtonPin, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  dht.begin();

  wifiSetup();
  otaSetup();
  connectAWS();
}

// ***********************************************************
void loop()
{
  ArduinoOTA.handle();
  checkInput();
  publishMessage();
  client.loop();
  delay(5000);
}