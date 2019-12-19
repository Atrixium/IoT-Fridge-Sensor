/*
 *  This sketch sends MQTT data to an MQTT broker, checks a thermistor for temperature and sends the data to the MQTT broker
 *  WiFiManager allows wireless configuration and it can be programmed over the air by navigating to "[IP]/setflag" which puts the ESP8266 into OTA mode for 15 seconds
 * 
*/

#include <ESP8266WiFi.h>

//MQTT
#include <PubSubClient.h>

//OTA stuff
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>

//WifiManager
#include <DNSServer.h>
#include <WiFiManager.h>  

//Credentials and broker info
#include "secrets.h"

const char* ssid     = STASSID;
const char* password = STAPSK;
const long readingInterval = 10000; //300000 = 5 mins
#define TOPIC "IoT/esp8266x2"

//Thermistor constants/variables
#define ThermistorPin A0
int Vo;
float R1 = 10000; //Resistor value of voltage divider constant (ohms)
float logR2, R2, T;
float c1 = 1.221849189e-3, c2 = 2.094372171e-4, c3 = 2.785700391e-7; //Steinhart-Hart coefficients from thermistor datasheet

#define Vsense 16
unsigned long last_reading = 0;

//OTA stuff
ESP8266WebServer server;
bool ota_flag = false;
uint16_t time_elapsed = 0;
//OTA stuff

WiFiClient WiFiClient;
// create MQTT object
PubSubClient client(WiFiClient);

void callback(char*, byte*, unsigned int);
float GetTemp();
bool onBatt = false;

void handleOTA();


void setup() {  

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(Vsense, INPUT);
  Serial.begin(115200);

  //WiFiManager
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP");
  Serial.println("connected to WiFi");
  //end WiFiManager

  //Handle OTA Programming
  handleOTA();
  //end OTA

//MQTT stuff

  client.setServer(BROKER, PORT);
  client.setCallback(callback);
  
}

void loop() {

  //Check for OTA event
  if(ota_flag)
  {
     Serial.println("OTA Flag set! Waiting for upload...");
    uint16_t time_start = millis();
    while(time_elapsed < 15000)
    {
      ArduinoOTA.handle();
      time_elapsed = millis() - time_start;
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(10);
    }
    ota_flag = false;
    
  }

  //Serial.println("loop");
  yield();
  if (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("", MQTT_USER, MQTT_PASS)) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe(TOPIC, 1);
      client.subscribe(TOPIC "test", 1);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  if (client.connected()){
    unsigned long currentMillis = millis();
    if(!last_reading || currentMillis - last_reading >= readingInterval) //delay between sends
    {
      float temp = GetTemp();
      last_reading = currentMillis;
      Serial.print("Publishing current temp of ");
      Serial.print(temp);
      Serial.print("C ");
      Serial.print("to ");
      Serial.print(TOPIC);
      Serial.println("/temperature.");
      client.publish(TOPIC "/temperature", String(temp).c_str());
      client.publish(TOPIC "/BatteryMode", String(onBatt).c_str());
    }
  }
  client.loop();

//Handle web requests
server.handleClient();

//blink LED
  if(!digitalRead(Vsense)) {
    onBatt = true;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
  }
  else{
    onBatt = false;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(500);
  }
}



void callback(char* topic, byte * data, unsigned int length) {
  // handle message arrived {

  //Print received string char at a time
  Serial.print(topic);
  Serial.print(": ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)data[i]);
  }
  Serial.println();

  //Do something based on data received. Example: if second character in string stream == F (as in oFf) turn LED off, else turn on. From Andreas Speiss #48
  if (data[1] == 'F')  {
    //clientStatus = 0;
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    //clientStatus = 1;
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

float GetTemp(){
  int Vo = analogRead(ThermistorPin);
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  T = T - 273.15;  //Convert from Kelvin to Celsius
}

void handleOTA(){
    ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

//Serve up route to restart from http
  server.on("/restart", [](){
    server.send(200,"text/plain", "Restarting..");
    delay(1000);
    ESP.restart();
  });

//Serve up setflag route to set OTA flag
  server.on("/setflag", [](){
    server.send(200,"text/plain", "OTA flag set!");
    ota_flag = true;
    time_elapsed = 0;
  });

server.begin();
}

