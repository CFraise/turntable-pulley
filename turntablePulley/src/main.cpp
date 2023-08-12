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
//#define MS3   0
#define DIR   13
#define STEP  12
#define SLEEP 4

#define TOG_SWITCH  14

#define MOTOR_DRIVER 1

//#define FULL_STEP       1
#define HALF_STEP       2
#define QUARTER_STEP    3
#define EIGHTH_STEP     4
#define SIXTEENTH_STEP  5

//#define STEPPER_NEMA17  1 //COMMENT OUT IF USING SEEED STEPPER

#ifdef STEPPER_NEMA17

  #define LIFT_HEIGHT       2500
  #define UP_ACCEL          600
  #define UP_SPEED          3500
  #define UP_STEP_MODE      EIGHTH_STEP
  #define DOWN_ACCEL        350
  #define DOWN_SPEED        2500
  #define DOWN_STEP_MODE    SIXTEENTH_STEP

#else //SEEED params

  #define LIFT_HEIGHT       2500*1000/200
  #define UP_ACCEL          500
  #define UP_SPEED          2000
  #define UP_STEP_MODE      QUARTER_STEP
  #define DOWN_ACCEL        500
  #define DOWN_SPEED        2000
  #define DOWN_STEP_MODE    QUARTER_STEP

#endif



int microstepMult = 1;

AccelStepper stepper = AccelStepper(MOTOR_DRIVER, STEP, DIR);

int setupOTA();
void recvMsg(uint8_t *data, size_t len);
void changeMicrostep(int microstepIndex);
void lightSleep();

int currentSwitchRead = LOW;
int lastSwitchRead = LOW;
bool moving = LOW;
long int millisWhenFinished = 0;

//Params to tune
int upMode = QUARTER_STEP;
int upSpeed = 1500;


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

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // WebSerial.begin(&server);
  // WebSerial.msgCallback(recvMsg);
  // server.begin();

  Serial.print("Lift Height is ");
  Serial.print(LIFT_HEIGHT);



  pinMode(TOG_SWITCH, INPUT);
  pinMode(MS1, OUTPUT);
  pinMode(MS2, OUTPUT);
  //pinMode(MS3, OUTPUT);
  pinMode(SLEEP, OUTPUT);

  // Start in eighth mode by default
  digitalWrite(MS1, HIGH);
  digitalWrite(MS2, HIGH);
  //digitalWrite(MS3, LOW);

  digitalWrite(SLEEP, HIGH); //Unpowered until switch goes high

  stepper.setCurrentPosition(0);
  stepper.setMaxSpeed(800);
  stepper.setAcceleration(400);
}

void loop() {
  //If it powers on or is switched to HIGH then the motor needs to move to a certain position. For now force it to go all the way up and then check for if things are low  
  //Will need to reposition code when enacting light sleep
  currentSwitchRead = digitalRead(TOG_SWITCH);

  if( currentSwitchRead && !lastSwitchRead) {
    digitalWrite(SLEEP, LOW); //enable motor driver
    changeMicrostep(UP_STEP_MODE);    
    moving = HIGH;
    Serial.println("Switched HIGH!");
    stepper.setMaxSpeed(UP_SPEED);
    stepper.setAcceleration(UP_ACCEL);
    stepper.moveTo(microstepMult*LIFT_HEIGHT);
  }

  // if( currentSwitchRead && stepper.currentPosition() < microstepMult*LIFT_HEIGHT ) {
  //   // if( stepper.currentPosition() < ((microstepMult*LIFT_HEIGHT) / 2) ) { //Faster near bottom
  //     stepper.setSpeed(UP_SPEED_MED);
  //     stepper.runSpeed();
  //   // } else { //Slower up top
  //   //   stepper.setSpeed(UP_SPEED_SLOW);
  //   //   stepper.runSpeed();
  //   // }
  //   Serial.println(stepper.currentPosition());
  // }

  if( !currentSwitchRead && lastSwitchRead ) { //&& stepper.currentPosition() == (microstepMult*LIFT_HEIGHT) ) {
    changeMicrostep(DOWN_STEP_MODE); 
    Serial.println("Switched LOW!");
    stepper.setMaxSpeed(DOWN_SPEED);
    stepper.setAcceleration(DOWN_ACCEL);
    stepper.moveTo(0);
  }

  if(moving)
    stepper.run();
  // if( !currentSwitchRead && stepper.currentPosition() > 0) { //Lower back down slowly
  //   // if( stepper.currentPosition() > ((microstepMult*LIFT_HEIGHT) / 2) ) { //Faster near top
  //     stepper.setSpeed(DOWN_SPEED_MED);
  //     stepper.runSpeed();
  //   // } else { //Slower near bottom
  //   //   stepper.setSpeed(DOWN_SPEED_SLOW);
  //   //   stepper.runSpeed();
  //   // }
  //   Serial.println(stepper.currentPosition());
  // }

  if(moving && !currentSwitchRead && stepper.currentPosition() == 0) {
    moving = LOW;
    digitalWrite(SLEEP, HIGH); //disable motor driver
    millisWhenFinished = millis();
  }

  if(millisWhenFinished != 0 && !moving && millis() > (millisWhenFinished + 5000)) { //5s after stopping NOTE should eventually change so this happens after startup as well. if it gets powered on and isnt used could end up being wasteful
    Serial.println("Entering SLEEP mode!!");
    delay(1000);
    lightSleep();
    delay(100);
    Serial.println("Wakey wakey??");
    //ESP.wdtEnable(5000);
  }


  lastSwitchRead = currentSwitchRead;
  //Serial.println("Awake");
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
    //upMode = FULL_STEP;
    WebSerial.println("Can't change to full step on TMC2208");
  }
  else if (d=="H"){
    upMode = HALF_STEP;
    WebSerial.println("Changed upward microstep mode to Half Step");
  }
  else if (d=="Q"){
    upMode = QUARTER_STEP;
    WebSerial.println("Changed upward microstep mode to Quarter Step");
  }
  else if (d=="E"){
    upMode = EIGHTH_STEP;
    WebSerial.println("Changed upward microstep mode to Eighth Step");
  }
  else if (d=="S"){
    upMode = SIXTEENTH_STEP;
    WebSerial.println("Changed upward microstep mode to Sixteenth Step");
  } else if (d=="U") {
    upSpeed += 100;
    WebSerial.print("Up speed is now to to: ");
    WebSerial.println(upSpeed);
  }else if (d=="I") {
    upSpeed += 50;
    WebSerial.print("Up speed is now to to: ");
    WebSerial.println(upSpeed);
  }else if (d=="O") {
    upSpeed += 1;
    WebSerial.print("Up speed is now to to: ");
    WebSerial.println(upSpeed);
  }else if (d=="B") {
    upSpeed -= 100;
    WebSerial.print("Up speed is now to to: ");
    WebSerial.println(upSpeed);
  }else if (d=="N") {
    upSpeed -= 50;
    WebSerial.print("Up speed is now to to: ");
    WebSerial.println(upSpeed);
  }else if (d=="0") {
    stepper.setAcceleration(100);
    WebSerial.println("Acceleration is now 100");
  }else if (d=="1") {
    stepper.setAcceleration(150);
    WebSerial.println("Acceleration is now 150");
  }else if (d=="2") {
    stepper.setAcceleration(200);
    WebSerial.println("Acceleration is now 200");
  }else if (d=="3") {
    stepper.setAcceleration(250);
    WebSerial.println("Acceleration is now 250");
  }else if (d=="4") {
    stepper.setAcceleration(300);
    WebSerial.println("Acceleration is now 300");
  }else if (d=="5") {
    stepper.setAcceleration(350);
    WebSerial.println("Acceleration is now 350");
  }else if (d=="6") {
    stepper.setAcceleration(600);
    WebSerial.println("Acceleration is now 600");
  }else if (d=="7") {
    stepper.setAcceleration(800);
    WebSerial.println("Acceleration is now 800");
  }else if (d=="8") {
    stepper.setAcceleration(1000);
    WebSerial.println("Acceleration is now 1000");
  }else if (d=="9") {
    stepper.setAcceleration(220);
    WebSerial.println("Acceleration is now 220");
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

// Adjusts the driver pins to enter a different microstep mode and changes the current steps accordingly
// input: microstepIndex -> chooses which microstep mode to enter
void changeMicrostep(int microstepIndex) {
  switch(microstepIndex) {
    // case FULL_STEP:
    //   digitalWrite(MS1, LOW);
    //   digitalWrite(MS2, LOW);
    //   digitalWrite(MS3, LOW);
    //   stepper.setCurrentPosition(  stepper.currentPosition() * 1 / microstepMult );
    //   microstepMult = 1;
    //   Serial.println("Switched to Full Step mode");
    //   break;
    case HALF_STEP:
      digitalWrite(MS1, HIGH);
      digitalWrite(MS2, LOW);
      //digitalWrite(MS3, LOW);
      stepper.setCurrentPosition(  stepper.currentPosition() * 2 / microstepMult );
      microstepMult = 2;
      Serial.println("Switched to Half Step mode");
      break;
    case QUARTER_STEP:
      digitalWrite(MS1, LOW);
      digitalWrite(MS2, HIGH);
      //digitalWrite(MS3, LOW);
      stepper.setCurrentPosition(  stepper.currentPosition() * 4 / microstepMult );
      microstepMult = 4;
      Serial.println("Switched to Quarter Step mode");
      break;
    case EIGHTH_STEP:
      digitalWrite(MS1, LOW);
      digitalWrite(MS2, LOW);
      //digitalWrite(MS3, LOW);
      stepper.setCurrentPosition(  stepper.currentPosition() * 8 / microstepMult );
      microstepMult = 8;
      Serial.println("Switched to Eighth Step mode");
      break;
    case SIXTEENTH_STEP:
      digitalWrite(MS1, HIGH);
      digitalWrite(MS2, HIGH);
      //digitalWrite(MS3, HIGH);
      stepper.setCurrentPosition(  stepper.currentPosition() * 16 / microstepMult );
      microstepMult = 16;
      Serial.println("Switched to Sixteenth Step mode");
      break;
  }
}

void lightSleep() {
  //ESP.wdtDisable();
  wifi_set_opmode(NULL_MODE); 
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  wifi_fpm_open();
  delay(100);
  gpio_pin_wakeup_enable(GPIO_ID_PIN(TOG_SWITCH), GPIO_PIN_INTR_HILEVEL);
  wifi_fpm_do_sleep(0xFFFFFFF);
  delay(100);
}
