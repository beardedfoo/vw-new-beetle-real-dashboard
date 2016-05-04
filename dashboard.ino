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

/*
  Transistor controlled wires to the back of the instrument cluster:
  These must be supplied +12v via a high-side switch
  from the arduino pins. Build a high-side NPN+PNP switch
  for each wire and assign the appropriate pins here. These are not simple
  logic signals, I have observed some of these wires sinking up to 115mA @ 12V.
  
  In addition to these transistor controlled wires the following connections must
  be permanently made:

    12V -> red/light purple (not red/dark purple!) on blue plug
    GND -> brown wire on blue plug

*/

/* 
  ignition wire:  light purple/black (not dark purple/black!) wire on blue plug

  When supplied +12v this wire boots up the instrument cluster as when the key is
  inserted into the ignition on the real car.
*/
const int PIN_DASH_IGNITION = 13;

/*
  illumination wire: light blue/dark blue wire on blue plug

  When supplied +12v this indicates the dash should be lit up, as happens when 
  turning on the headlights in the real car.

*/
const int PIN_DASH_ILLUM = 12;

/*
  Definitions for the stock stepper motors which drive the gauges in the instrument cluster.
  The motor pins must be detached from the instrument cluster PCB and wires soldered
  to a motor driver connected to the arduino; I am using the AdaFruit Motor shield v2.0. The
  motors consume about 100ma @ 5V total if all three are running.
*/
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
const float FUEL_SCALE = 3.25; // Steps per fuel percentage
const float SPEED_SCALE_MPH = 1.785; // Steps per mph

/*
  Adafruit motor shields only support two motors each. Solder the jumper
  on one of the boards so that it takes the 0x61 address.
*/
Adafruit_MotorShield motorBoard0 = Adafruit_MotorShield(0x60);
Adafruit_MotorShield motorBoard1 = Adafruit_MotorShield(0x61);

/*
  Connect two motors to the first board and one to the second one.
  Define which motors are connected to what here. The second parameter
  is either 1 or 2, indicating which screw terminals the motor is connected
  to on the motor shield. Power the motor shield driver chips with +5v.
*/
Adafruit_StepperMotor *fuelMotor = motorBoard0.getStepper(STEPS, 1);
Adafruit_StepperMotor *tachMotor = motorBoard0.getStepper(STEPS, 2);
Adafruit_StepperMotor *speedoMotor = motorBoard1.getStepper(STEPS, 1);

/*
  Forward/backward controls for the gauge steppers. If some of your gauges 
  are reversed flip the lambda functions around here or switch the terminals
  on the motor shield.
*/
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
    // Provide remote echo. This can be removed if performance becomes a problem
    Serial.print(inputString);
    
    // Take commands as "[key]=[value]"
    int sepIndex = inputString.indexOf('=');
    if (sepIndex > 0) {
      String key = inputString.substring(0, sepIndex);
      String value = inputString.substring(sepIndex+1);
      
      // Act on commands and respond "ok"
      
      // Take speed as either miles or km and position gauge
      if (key == "mph") {
        Serial.print("o");
        setSpeedMPH(value.toInt());
        Serial.println("k");
      } else if (key == "kmh") {
        Serial.print("o");
        setSpeedKMH(value.toInt());
        Serial.println("k");
      
      // Set tach position as full rpm count
      } else if (key == "rpm") {
        Serial.print("o");
        setRPM(value.toInt());
        Serial.println("k");
        
      // Set fuel as a percentage
      } else if (key == "fuel") {
        Serial.print("o");
        setFuel(value.toInt());
        Serial.println("k");
        
      // Respond "?" with usage info for unknown commands
      } else {
        Serial.println("? [rpm|kmh|fuel|mph]=val");
      }
    } else {
      Serial.println("? [rpm|kmh|fuel|mph]=val");
    }
    
    // Prepare values for next command to be received
    inputString = "";
    inputReady = false;
  }
}

void loop() {
  // Look for and parse any available input commands
  parseInput();
  
  // Move motors towards their current target positions
  speedo.run();
  tach.run();
  fuel.run();
}
