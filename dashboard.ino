/*

  VW New Beetle (1999-2011) real dashboard for drivings simulators.

  Author: Cyle Riggs <beardedfoo@gmail.com>
  
  Commands are sent to the arduino board via serial @ 115200 baud using '\n' for line endings, such as:
    rpm=1500
    mph=42
    fuel=100

  This was developed on an arduino mega 2560.
  
*/


#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include <AccelStepper.h>

// Transistor controlled wires to the dash
const int PIN_DASH_IGNITION = 13;
const int PIN_DASH_ILLUM = 12;

// Definitions for the stock stepper motors which drive the gauges in the instrument cluster.
// The motor pins must be detached from the instrument cluster PCB and wires soldered
// to a motor driver connected to the arduino; I am using the AdaFruit Motor shield v2.0. The
// motors consume about 100ma @ 5V total if all three are running.
const int STEP_MODE = SINGLE;
const int STEPS = 500;

// Offset from leftmost point on gauge
const int TACH_ZERO = 19;
const int TACH_SPEED = 750;
const int TACH_ACCEL = 500;

const int FUEL_ZERO = 17;
const int FUEL_SPEED = 125;
const int FUEL_ACCEL = 100;

const int SPEEDO_ZERO = 13;
const int SPEEDO_SPEED = 250;
const int SPEEDO_ACCEL = 250;

const float TACH_SCALE = 0.057; // RPM per steps
const float FUEL_SCALE = 3.25;
const float SPEED_SCALE_MPH = 1.785; // Steps per mph

Adafruit_MotorShield motorBoard0 = Adafruit_MotorShield(0x60);
Adafruit_MotorShield motorBoard1 = Adafruit_MotorShield(0x61);

Adafruit_StepperMotor *speedoMotor = motorBoard1.getStepper(STEPS, 1);
Adafruit_StepperMotor *fuelMotor = motorBoard0.getStepper(STEPS, 1);
Adafruit_StepperMotor *tachMotor = motorBoard0.getStepper(STEPS, 2);

AccelStepper speedo([]{speedoMotor->onestep(FORWARD, STEP_MODE);},
                    []{speedoMotor->onestep(BACKWARD, STEP_MODE);});
AccelStepper tach([]{tachMotor->onestep(FORWARD, STEP_MODE);},
                  []{tachMotor->onestep(BACKWARD, STEP_MODE);});
AccelStepper fuel([]{fuelMotor->onestep(BACKWARD, STEP_MODE);}, // Fuel gauge is wired backwards, oops...
                  []{fuelMotor->onestep(FORWARD, STEP_MODE);});

// Serial input
bool inputReady;
String inputString;

// Convenince functions for translating values to steps
void setSpeedMPH(float mph) {
  speedo.moveTo(mph * SPEED_SCALE_MPH);
}

void setSpeedKMH(float kmh) {
  setSpeedMPH(kmh * 0.62137);
}

void setRPM(float rpm) {
  tach.moveTo(rpm * TACH_SCALE);
}

void setFuel(int percent) {
  fuel.moveTo(percent * FUEL_SCALE);
}

void setup() {
  // Start up the instrument cluster
  pinMode(PIN_DASH_IGNITION, OUTPUT);
  pinMode(PIN_DASH_ILLUM, OUTPUT);
  digitalWrite(PIN_DASH_IGNITION, HIGH);
  digitalWrite(PIN_DASH_ILLUM, HIGH);
  
  // Init the i2c boards
  // THIS MUST HAPPEN AT DEFAULT i2c CLOCK!!!
  motorBoard0.begin();
  motorBoard1.begin();
  
  // Change the i2c clock to 400KHz
  TWBR = ((F_CPU /400000l) - 16) / 2;
  
  speedo.setMaxSpeed(SPEEDO_SPEED);
  speedo.setAcceleration(SPEEDO_ACCEL);
  speedo.move(-STEPS);
  
  tach.setMaxSpeed(TACH_SPEED);
  tach.setAcceleration(TACH_ACCEL);
  tach.move(-STEPS);
  
  fuel.setMaxSpeed(FUEL_SPEED);
  fuel.setAcceleration(FUEL_ACCEL);
  fuel.move(-STEPS);
  
  while (tach.distanceToGo() != 0 || speedo.distanceToGo() != 0 || fuel.distanceToGo() != 0) {
    speedo.run();
    tach.run();
    fuel.run();
  }
  
  // Move all gauges to new zero
  speedo.move(SPEEDO_ZERO);
  tach.move(TACH_ZERO);
  fuel.move(FUEL_ZERO);
  
  while (tach.distanceToGo() != 0 || speedo.distanceToGo() != 0 || fuel.distanceToGo() != 0) {
    speedo.run();
    tach.run();
    fuel.run();
  }
  
  // Remember this spot as absolute zero
  speedo.setCurrentPosition(0);
  tach.setCurrentPosition(0);
  fuel.setCurrentPosition(0);

  // Gauges rest @ 0
  setRPM(0);
  setSpeedMPH(0);
  setFuel(0);

  // Ready for input!
  Serial.begin(115200);
  inputString.reserve(12);
  inputReady = false;
  
}

// On interrupt from serial port read characters to build up a command line
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      inputReady = true;
    }
  }
}

// Parse a command line when it is ready
void parseInput() {
  if (inputReady) {
    Serial.print(inputString);
    int sepIndex = inputString.indexOf('=');
    if (sepIndex > 0) {
      String key = inputString.substring(0, sepIndex);
      String value = inputString.substring(sepIndex+1);
      
      if (key == "mph") {
        Serial.print("o");
        setSpeedMPH(value.toInt());
        Serial.println("k");
      } else if (key == "kmh") {
        Serial.print("o");
        setSpeedKMH(value.toInt());
        Serial.println("k");
      } else if (key == "rpm") {
        Serial.print("o");
        setRPM(value.toInt());
        Serial.println("k");
      } else if (key == "fuel") {
        Serial.print("o");
        setFuel(value.toInt());
        Serial.println("k");
      } else {
        Serial.println("?");
      }
    } else {
      Serial.println("? [rpm|kmh|fuel|mph]=val");
    }
    inputString = "";
    inputReady = false;
  }
}

void loop() {
  parseInput();
  
  speedo.run();
  tach.run();
  fuel.run();
}
