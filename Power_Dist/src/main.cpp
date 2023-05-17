#include <Arduino.h>

#include <SPI.h>
#include <SD.h>

#include <Adafruit_PCT2075.h>

Adafruit_PCT2075 MCTherm;
Adafruit_PCT2075 RegTherm;

// Pin Definitions
#define BATT_1_VSENSE A1
#define BATT_1_CURR A2
#define BATT_1_INT 4
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

volatile bool batt1On;
volatile bool batt2On;
bool batt1Installed;
bool batt2Installed;
float batt1V;
float batt2V;

#define SWITCHING_GRACE 1000000 // isr trigger grace period in micros
volatile bool ISR_Override;
volatile unsigned long prevSwitchTime;

// error flags
bool dualLowErrorFlag;
bool MCThermErrorFlag;
bool RegThermErrorFlag;

// Buzzer stuff
bool buzzOn;
unsigned long buzzerIntervalLive; 
unsigned long prevBuzz;
#define BUZZ_INTERVAL_RAPID   200000
#define BUZZ_INTERVAL_SLOW    1000000
#define BUZZ_FULL             1
#define BUZZ_OFF              0

void updateSD();
float calcVBatt(float vsense);
void batt1ISR();
void batt2ISR();
void pollBatteries();
bool inSwitchingTimeout();

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ;
  }

  pinMode(BATT_1_VSENSE, INPUT_PULLDOWN);
  pinMode(BATT_1_CURR, INPUT);
  pinMode(BATT_1_INT, INPUT_PULLUP);
  pinMode(BATT_1_CTL, OUTPUT);

  pinMode(BATT_2_VSENSE, INPUT_PULLDOWN);
  pinMode(BATT_2_CURR, INPUT);
  pinMode(BATT_2_INT, INPUT_PULLUP);
  pinMode(BATT_2_CTL, OUTPUT);

  pinMode(A0_SENSE, INPUT);
  pinMode(A5_SENSE, INPUT);
  pinMode(A5_SENSE, BUZZER);

  // attachInterrupt(digitalPinToInterrupt(BATT_1_INT), batt1ISR, FALLING);
  // attachInterrupt(digitalPinToInterrupt(BATT_2_INT), batt2ISR, FALLING);

  batt1On = false;
  batt2On = false;
  pollBatteries();
  Serial.println("Read Batt 1:" + String(batt1V));
  Serial.println("Read Batt 2:" + String(batt2V));

  // Not equal to because there is a possibility that neither battery is actually plugged in (just mcu)
  if (batt1V > batt2V) {
    digitalWrite(BATT_1_CTL, HIGH);
    batt1On = true;
    Serial.println("Selected batt 1!");
  }
  else {
    digitalWrite(BATT_2_CTL, HIGH);
    batt2On = true;
    Serial.println("Selected batt 2!");
  }

  prevSwitchTime = micros();
  ISR_Override = false;

  dualLowErrorFlag = false;
  MCThermErrorFlag = false;
  RegThermErrorFlag = false;
  buzzerIntervalLive = BUZZ_OFF;
  buzzOn = false;
  prevBuzz = micros();

  sdPrevUpdate = micros();
  

  if (!SD.begin(SD_CS)) {
    Serial.println("Connection to SD Card failed");
  }
  file = SD.open("log.txt", FILE_WRITE);
  
  // Initialize Thermometers
  if (!MCTherm.begin(0x48, &Wire))
    file.println("MCTherm init failed\n");
  if (!RegTherm.begin(0x37, &Wire))
    file.println("RegTherm init failed\n");
}

void loop() {

  pollBatteries();


  if (!inSwitchingTimeout()) {

    // In the case that both batteries dropped low and then one was replaced
    if (batt1On && batt2On) {
      if (batt1V > BATT_EMPTY) {
        digitalWrite(BATT_2_CTL, LOW);
        batt2On = false;
        prevSwitchTime = micros();
        Serial.println("Selected Batt 1!");
      }
      if (batt2V > BATT_EMPTY) {
        digitalWrite(BATT_1_CTL, LOW);
        batt1On = false;
        prevSwitchTime = micros();
        Serial.println("Selected Batt 2!");
      }
    }

    if (batt1On && batt1V < BATT_EMPTY && batt2Installed && !batt2On) {
      ISR_Override = true;
      digitalWrite(BATT_2_CTL, HIGH);
      batt2On = true;
      prevSwitchTime = micros();
      Serial.println("Switched to batt 2!");
      if (batt2V > BATT_EMPTY) {
        digitalWrite(BATT_1_CTL, LOW);
        batt1On = false;
        prevSwitchTime = micros();
      }
      ISR_Override = false;
    }
    if (batt2On && batt2V < BATT_EMPTY && batt1Installed && !batt1On) {
      ISR_Override = true;
      digitalWrite(BATT_1_CTL, HIGH);
      batt1On = true;
      prevSwitchTime = micros();
      Serial.println("Switched to batt 1!");
      if (batt1V > BATT_EMPTY) {
        digitalWrite(BATT_2_CTL, LOW);
        batt2On = false;
        prevSwitchTime = micros();
      }
      ISR_Override = false;
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

  if (buzzerIntervalLive == BUZZ_FULL) {
    digitalWrite(BUZZER, HIGH);
  }
  else if (buzzerIntervalLive == BUZZ_OFF) {
    digitalWrite(BUZZER, LOW);
  }
  else if ((micros() - prevBuzz) > buzzerIntervalLive) {
    prevBuzz = micros();
    buzzOn = !buzzOn;
    digitalWrite(BUZZER, buzzOn);
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

void batt1ISR() {
  if (batt2Installed && !inSwitchingTimeout() && !ISR_Override) {
    digitalWrite(BATT_2_CTL, HIGH);
    digitalWrite(BATT_1_CTL, LOW);
    batt2On = true;
    prevSwitchTime = micros();
    Serial.println("Batt 1 removed, switched to 2");
  }
  else {
    // fuck
    file.println("Batt 1 disconnect while Batt2 uninstalled");
  }
}

void batt2ISR() {
  if (batt1Installed && !inSwitchingTimeout() && !ISR_Override) {
    digitalWrite(BATT_1_CTL, HIGH);
    digitalWrite(BATT_2_CTL, LOW);
    batt1On = true;
    prevSwitchTime = micros();
    Serial.println("Batt 2 removed, switched to 1");
  }
  else {
    // fuck
    file.println("Batt 2 disconnect while Batt1 uninstalled");
  }
}

// Calculates the vsense pin voltage to the battery voltage
float calcVBatt(float vsense) { 
  return vsense / (2.54 / 14.94);
}

void pollBatteries() {
  batt1V = calcVBatt(3.3 * analogRead(BATT_1_VSENSE) / 1048);
  batt2V = calcVBatt(3.3 * analogRead(BATT_2_VSENSE) / 1048);

  batt1Installed = batt1V > 0.5;
  batt2Installed = batt2V > 0.5;
}


bool inSwitchingTimeout() {
  return (micros() - prevSwitchTime) < SWITCHING_GRACE;
}