#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <AccelStepper.h>

#include "credentials.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWD;

AsyncWebServer server(80);

// A4988 GPIOs
#define MS1   15
#define MS2   2
#define MS3   0
#define DIR   13
#define STEP  12

#define TOG_SWITCH  14

#define MOTOR_DRIVER 1

#define LIFT_HEIGHT       400
#define UP_SPEED_SLOW     100
#define UP_SPEED_MED      200
#define DOWN_SPEED_SLOW   -50
#define DOWN_SPEED_MED    -100

AccelStepper stepper = AccelStepper(MOTOR_DRIVER, STEP, DIR);

int setupOTA();
void recvMsg(uint8_t *data, size_t len);

int lastSwitch = LOW;


void setup() {
  Serial.begin(115200);
  Serial.println("Beginning Turntable Pulley test...");

  Serial.println("Connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if( setupOTA() ) {
    WebSerial.begin(&server);
    WebSerial.msgCallback(recvMsg);
    server.begin();
  }


  pinMode(TOG_SWITCH, INPUT);
  pinMode(MS1, OUTPUT);
  pinMode(MS2, OUTPUT);
  pinMode(MS3, OUTPUT);

  // Start in fullstep mode by default
  digitalWrite(MS1, LOW);
  digitalWrite(MS2, LOW);
  digitalWrite(MS3, LOW);

  stepper.setCurrentPosition(0);
  stepper.setMaxSpeed(1000);
}

void loop() {
  //If it powers on or is switched to HIGH then the motor needs to move to a certain position. For now force it to go all the way up and then check for if things are low
  // if(digitalRead(TOG_SWITCH) && stepper.currentPosition() < LIFT_HEIGHT ) {
  //   if( stepper.currentPosition() < (LIFT_HEIGHT / 2) ) { //Faster near bottom
  //     stepper.setSpeed(UP_SPEED_MED);
  //     stepper.runSpeed();
  //   } else { //Slower up top
  //     stepper.setSpeed(UP_SPEED_SLOW);
  //     stepper.runSpeed();
  //   }
  // }

  // if( !digitalRead(TOG_SWITCH) && stepper.currentPosition() > 0) { //Lower back down slowly
  //   if( stepper.currentPosition() > (LIFT_HEIGHT / 2) ) { //Faster near top
  //     stepper.setSpeed(DOWN_SPEED_MED);
  //     stepper.runSpeed();
  //   } else { //Slower near bottom
  //     stepper.setSpeed(DOWN_SPEED_SLOW);
  //     stepper.runSpeed();
  //   }
  // }
  if( digitalRead(TOG_SWITCH) && lastSwitch == LOW) {
    Serial.println("Switched HIGH!");
    while(stepper.currentPosition() < LIFT_HEIGHT ) {
      stepper.setSpeed(UP_SPEED_MED);
      stepper.runSpeed();
    }
  }

  if( !digitalRead(TOG_SWITCH) && lastSwitch == HIGH) {
    Serial.println("Switched LOW!");
    while(stepper.currentPosition() > 0 ) {
      stepper.setSpeed(-1*UP_SPEED_MED);
      stepper.runSpeed();
    }
  }


  // lastSwitch = digitalRead(TOG_SWITCH);
  // Serial.println(lastSwitch);

  ArduinoOTA.handle();
  yield();  
}

void recvMsg(uint8_t *data, size_t len){
  WebSerial.println("Received Data...");
  String d = "";
  for(unsigned int i=0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
  if (d == "F"){ 
    digitalWrite(MS1, LOW);
    digitalWrite(MS2, LOW);
    digitalWrite(MS3, LOW);
    WebSerial.println("Changed to Full Step Mode");
  }
  if (d=="H"){
    digitalWrite(MS1, HIGH);
    digitalWrite(MS2, LOW);
    digitalWrite(MS3, LOW);
    WebSerial.println("Changed to Half Step Mode");
  }
  if (d=="Q"){
    digitalWrite(MS1, LOW);
    digitalWrite(MS2, HIGH);
    digitalWrite(MS3, LOW);
    WebSerial.println("Changed to Quarter Step Mode");
  }
  if (d=="E"){
    digitalWrite(MS1, HIGH);
    digitalWrite(MS2, HIGH);
    digitalWrite(MS3, LOW);
    WebSerial.println("Changed to Eighth Step Mode");
  }
  if (d=="S"){
    digitalWrite(MS1, HIGH);
    digitalWrite(MS2, HIGH);
    digitalWrite(MS3, HIGH);
    WebSerial.println("Changed to Sixteenth Step Mode");
  }
}

// Initialize the WiFi and prepare board for OTA updates
// Return: 1 if connection succeeded or -1 if it failed
int setupOTA()
{ 
  // if(WiFi.waitForConnectResult() != WL_CONNECTED)
  // {
  //   Serial.println("Connection Failed! Trying home credentials");
  //   WiFi.begin(ssid2, password2);
  // }
  if(WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! No OTA updates available...");
    return -1;
    // delay(5000);
    // ESP.restart();
  }
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
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  return 1;
}
