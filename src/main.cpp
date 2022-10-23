#include <Arduino.h>
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Wifi.h"
#include "DHT.h"
#include <Stepper.h>

//DHT Sensor Config
#define DHTPIN 16
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
float humidity;
float temperature;
int desiredTemperature = 0;

//LED Pins
#define PIN_RED    23 // GIOP23
#define PIN_GREEN  22 // GIOP22
#define PIN_BLUE   21 // GIOP21

//Stepper Configuration
const int stepsPerRevolution = 200;
Stepper myStepper(stepsPerRevolution, 32, 33, 34, 35);

//AWS & WiFi Config
#define AWS_IOT_PUBLISH_TOPIC   "evryhub/vent"
#define AWS_IOT_SUBSCRIBE_TOPIC "evryhub/vent"
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

void openVent(){
  myStepper.step(stepsPerRevolution);
  delay(500);
}

void closeVent(){
  myStepper.step(-stepsPerRevolution);
  delay(500);
}

void configVent(String value){
  desiredTemperature = value.toInt();
  delay(500);
}

void messageHandler(char* topic, byte* payload, unsigned int length)
{
  Serial.print("incoming: ");
  Serial.println(topic);

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);

  const char* d = doc["device"];
  const char* a = doc["action"];
  const char* v = doc["value"];

  String device = String(d);
  String action = String(a);
  String value  = String(v);

  Serial.printf("\n Device: %s | Action: %s \n", device, action);

  if(device == "vent" && action == "config"){
    Serial.println("Configure EvryHub Vent");
    configVent(value);
  }else if(device == "vent" && String(action) == "open"){
    Serial.println("Open Vent");
    openVent();
  }else if(device == "vent" && String(action) == "close"){
    Serial.println("Close Vent");
    closeVent();
  }
}

void connectAWS()
{
  //WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // LED Turns Green When Connected to WiFi
  analogWrite(PIN_RED, 0);
  analogWrite(PIN_GREEN, 255);
  analogWrite(PIN_BLUE, 0);

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

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");

  // LED Turns Blue When Connected to AWS
  analogWrite(PIN_RED, 0);
  analogWrite(PIN_GREEN, 0);
  analogWrite(PIN_BLUE, 255);
}

void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["device"] = "vent";
  doc["action"] = "temperature:"+String(temperature)+";humidity:"+String(humidity);
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void setup() {
  myStepper.setSpeed(60);
  Serial.begin(115200);

  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);

  // LED Turns RED When Device is Turned On
  analogWrite(PIN_RED, 255);
  analogWrite(PIN_GREEN, 0);
  analogWrite(PIN_BLUE, 0);
  
  dht.begin();
  
  connectAWS();
}

void loop() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println(F("Failed to read from DHT sensor!"));
  }

  if(desiredTemperature != 0){
    if(temperature < desiredTemperature){
      openVent();
    }else{
      closeVent();
    }
  }

  publishMessage();
  client.loop();
  delay(5000);
}