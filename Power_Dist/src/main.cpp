#include <Arduino.h>

#include <SPI.h>
#include <SD.h>

#include <Adafruit_PCT2075.h>

Adafruit_PCT2075 MCTherm;
Adafruit_PCT2075 RegTherm;

// Pin Definitions
#define BATT_1_VSENSE A1
#define BATT_1_CURR A2
#define BATT_1_INT 3
#define BATT_1_CTL 10

#define BATT_2_VSENSE A4
#define BATT_2_CURR A3
#define BATT_2_INT 9
#define BATT_2_CTL 13

#define A0_SENSE A0
#define A5_SENSE A5
#define BUZZER 11
#define SD_CS 2

// Other stuff
#define BATT_EMPTY 12.8 // Empty voltage
#define MAX_TEMP 40 // Temperature at which an error is logged in C

File file;
String buffer;

unsigned long sdPrevUpdate;

bool batt1On;
bool batt2On;
volatile bool batt1Installed;
volatile bool batt2Installed;
float batt1V;
float batt2V;

// error flags
bool dualLowErrorFlag;
bool MCThermErrorFlag;
bool RegThermErrorFlag;

void updateSD();
float calcVBatt(float vsense);
void batt1ISR();
void batt2ISR();
void pollBatteries();

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ;
  }

  if (!SD.begin(SD_CS)) {
    Serial.println("Connection to SD Card failed");
  }
  file = SD.open("log.txt", FILE_WRITE);
  
  // Initialize Thermometers
  if (!MCTherm.begin(0x48, &Wire))
    file.println("MCTherm init failed\n");
  // if (!RegTherm.begin(0x37, &Wire))
  //   file.println("RegTherm init failed\n");

  pinMode(BATT_1_VSENSE, INPUT);
  pinMode(BATT_1_CURR, INPUT);
  pinMode(BATT_1_INT, INPUT_PULLUP);
  pinMode(BATT_1_CTL, OUTPUT);

  pinMode(BATT_2_VSENSE, INPUT);
  pinMode(BATT_2_CURR, INPUT);
  pinMode(BATT_2_INT, INPUT_PULLUP);
  pinMode(BATT_2_CTL, OUTPUT);

  pinMode(A0_SENSE, INPUT);
  pinMode(A5_SENSE, INPUT);
  pinMode(A5_SENSE, BUZZER);

  attachInterrupt(digitalPinToInterrupt(BATT_1_INT), batt1ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BATT_2_INT), batt2ISR, FALLING);

  batt1On = false;
  batt2On = false;
  pollBatteries();

  if (batt1V > batt2V) {
    digitalWrite(BATT_1_CTL, HIGH);
    batt1On = true;
  }
  else {
    digitalWrite(BATT_2_CTL, HIGH);
    batt2On = true;
  }

  dualLowErrorFlag = false;
  MCThermErrorFlag = false;
  RegThermErrorFlag = false;

  sdPrevUpdate = micros();
}

void loop() {

  pollBatteries();

  if (batt1On && batt1V < BATT_EMPTY && batt2Installed && !batt2On) {
    digitalWrite(BATT_2_CTL, HIGH);
    batt2On = true;
    if (batt2V > BATT_EMPTY) {
      digitalWrite(BATT_1_CTL, LOW);
      batt1On = false;
    }
  }
  if (batt2On && batt2V < BATT_EMPTY && batt1Installed && !batt1On) {
    digitalWrite(BATT_1_CTL, HIGH);
    batt1On = true;
    if (batt1V > BATT_EMPTY) {
      digitalWrite(BATT_2_CTL, LOW);
      batt2On = false;
    }
  }

  // Errors
  if (batt1Installed && batt1V < BATT_EMPTY && batt2Installed && batt2V < BATT_EMPTY) {
    if (!dualLowErrorFlag) {
      buffer += "Both batteries empty\n";
      dualLowErrorFlag = true;
    }
  }
  else {
    dualLowErrorFlag = false;
  }

  if (MCTherm.getTemperature() > MAX_TEMP) {
    if (!MCThermErrorFlag) {
      buffer += "MC Thermometer logged: " + (String)MCTherm.getTemperature() + "\n";
      MCThermErrorFlag = true;
    }
  }
  else {
    MCThermErrorFlag = false;
  }

  if (RegTherm.getTemperature() > MAX_TEMP) {
    if (!RegThermErrorFlag) {
      buffer += "Reg Thermometer logged: " + (String)RegTherm.getTemperature() + "\n";
      RegThermErrorFlag = true;
    }
  }
  else {
    RegThermErrorFlag = false;
  }

  if (micros() - sdPrevUpdate > 100)
    updateSD();
}


// Idea is the log everything to the buffer variable and then to `updateSD()` at the end of the loop to save time
void updateSD(void) {
  if (buffer.length() > 0) {
    file.println(buffer);
  }
  buffer.remove(0);
}

// Calculates the vsense pin voltage to the battery voltage
float calcVBatt(float vsense) { 
  return vsense / (4.7 / 26.7);
}

void batt1ISR() {
  if (batt2Installed) {
    digitalWrite(BATT_2_CTL, HIGH);
  }
  else {
    // fuck
    file.println("Batt1 disconnect while Batt2 uninstalled");
  }
}

void batt2ISR() {
  if (batt1Installed) {
    digitalWrite(BATT_1_CTL, HIGH);
  }
  else {
    // fuck
    file.println("Batt2 disconnect while Batt1 uninstalled");
  }
}


void pollBatteries() {
  batt1V = calcVBatt(3.3 * analogRead(BATT_1_VSENSE) / 1048);
  batt2V = calcVBatt(3.3 * analogRead(BATT_2_VSENSE) / 1048);

  batt1Installed = batt1V > 0;
  batt2Installed = batt2V > 0;
}