#include <Arduino.h>

#include <SPI.h>
#include <SD.h>

#include <Adafruit_PCT2075.h>

#include <BuzzerTimer.h>


// Pin Definitions
#define BATT_1_VSENSE A1
#define BATT_1_CURR 2
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
#define SERIAL_1_RX 0
#define SERIAL_1_TX 1

// Other stuff
#define BATT_EMPTY 14 // Empty voltage
#define MAX_TEMP 60 // Temperature at which an error is logged in C

File file;
String buffer;

unsigned long sdPrevUpdate;

volatile bool batt1On;
volatile bool batt2On;
bool batt1Installed;
bool batt2Installed;
float batt1V;
float batt2V;
float batt1Curr;
float batt2Curr;
#define CURR_SENSE_INTERVAL     1000000
#define CURR_SENSE_VOLTAGE_MULT 11
#define CURR_SENSE_CURR_MULT    56.81818
#define CURR_SENSE_VOLT_OFFSET  0
#define CURR_SENSE_CURR_OFFSET  0.330
unsigned long prevCurrSense;
float batt1VFIFO[10];
float batt2VFIFO[10];


#define HIGH_CURRENT    50 //Amps
bool batt1CurrErrorFlag;
bool batt2CurrErrorFlag;
bool dualLowErrorFlag;


#define SWITCHING_GRACE 1000000 // isr trigger grace period in micros
volatile bool ISR_Override;
volatile unsigned long prevSwitchTime;

// Kill switch = arming switch
#define KILL_SWITCH_GRACE   500000
unsigned long armingTimer; 
bool armingWaiting;
bool armedWaitingState;
bool armed;


Adafruit_PCT2075 MCTherm;
Adafruit_PCT2075 RegTherm;
bool MCThermErrorFlag;
bool RegThermErrorFlag;

// Buzzer stuff
bool buzzOn;
unsigned long prevBuzz;
#define BUZZ_INTERVAL_RAPID   100000
#define BUZZ_INTERVAL_SLOW    200000
#define BUZZ_FULL             1
#define BUZZ_OFF              0

BuzzerTimer buzzerTimer;

float calcVBatt(float vsense);
void batt1ISR();
void batt2ISR();
void pollBatteries();
bool inSwitchingTimeout();
float getBatt1Curr();
float getBatt2Curr();

void setup() {
  Serial.begin(9600);
  // while (!Serial) {
  //   ;
  // }

  pinMode(BATT_1_VSENSE, INPUT_PULLDOWN);
  pinMode(BATT_1_CURR, INPUT_PULLDOWN);
  pinMode(BATT_1_INT, INPUT_PULLUP);
  pinMode(BATT_1_CTL, OUTPUT);

  pinMode(BATT_2_VSENSE, INPUT_PULLDOWN);
  pinMode(BATT_2_CURR, INPUT_PULLDOWN);
  pinMode(BATT_2_INT, INPUT_PULLUP);
  pinMode(BATT_2_CTL, OUTPUT);

  pinMode(A0_SENSE, INPUT);
  pinMode(A5_SENSE, INPUT);
  pinMode(SERIAL_1_RX, INPUT_PULLUP);

  // attachInterrupt(digitalPinToInterrupt(BATT_1_INT), batt1ISR, FALLING);
  // attachInterrupt(digitalPinToInterrupt(BATT_2_INT), batt2ISR, FALLING);

  batt1On = false;
  batt2On = false;
  pollBatteries();
  digitalWrite(BATT_1_CTL, LOW);
  digitalWrite(BATT_2_CTL, LOW);


  Serial.println("Read Batt 1:" + String(batt1V));
  Serial.println("Read Batt 2:" + String(batt2V));

  batt1Curr = 0;
  batt2Curr = 0;
  prevCurrSense = micros();
  for (int x = 0; x < sizeof(batt1VFIFO) / sizeof(float); x++) {
    batt1VFIFO[x] = batt1V;
    batt2VFIFO[x] = batt2V;
  }


  buffer = "";

  // Not equal to because there is a possibility that neither battery is actually plugged in (just mcu)
  // if (batt1V > batt2V) {
  //   digitalWrite(BATT_1_CTL, HIGH);
  //   batt1On = true;
  //   Serial.println("Selected batt 1!");
  //   buffer += "Selected batt 1 at init";
  // }
  // else {
  //   digitalWrite(BATT_2_CTL, HIGH);
  //   batt2On = true;
  //   Serial.println("Selected batt 2!");
  //   buffer += "Selected batt 2 at init";
  // }

  prevSwitchTime = micros();
  ISR_Override = false;

  armingWaiting = false;
  armingTimer = micros();
  armed = digitalRead(SERIAL_1_RX);
  armedWaitingState = false;

  batt1CurrErrorFlag = false;
  batt2CurrErrorFlag = false;
  dualLowErrorFlag = false;

  MCThermErrorFlag = false;
  RegThermErrorFlag = false;
  buzzOn = true;
  prevBuzz = micros();

  sdPrevUpdate = micros();
  

  if (!SD.begin(SD_CS)) {
    Serial.println("Connection to SD Card failed");
  }
  if (SD.exists("log.txt"))
    SD.remove("log.txt");
  file = SD.open("log.txt", FILE_WRITE);
  
  file.println("Onyx Unified PDB #2");

  // Initialize Thermometers
  if (!MCTherm.begin(0x48, &Wire)) {
    file.println("MCTherm init failed");
  }
  if (!RegTherm.begin(0x37, &Wire))
    file.println("RegTherm init failed");

  buzzerTimer.set(BUZZ_INTERVAL_SLOW, 4);
  file.flush();
}

void loop() {

  pollBatteries();

  // Handle the kill switch / arming and disarming

  // if (((!digitalRead(SERIAL_1_RX) && armed) || (digitalRead(SERIAL_1_RX) && !armed)) && !armingWaiting) {
  //   armingWaiting = true;
  //   armedWaitingState = digitalRead(SERIAL_1_RX);
  //   armingTimer = micros();
  // }

  // if (armingWaiting && (micros() - armingTimer) > KILL_SWITCH_GRACE) {
  //   armingWaiting = false;
  //   armingTimer = micros();
  //   if (digitalRead(SERIAL_1_RX) == armedWaitingState) {
  //     armed = !armed;
  //     if (!armed) {
  //       buffer += "Disarmed";
  //       Serial.println("Disarmed");
  //     }
  //     else if (armed) {
  //       buffer += "armed";
  //       Serial.println("Armed");
  //     }
  //   }
  // }


  if ((micros() - armingTimer) > KILL_SWITCH_GRACE) {
    armed = digitalRead(SERIAL_1_RX);
    armingTimer = micros();
  }

  if (!armed) {
    digitalWrite(BATT_1_CTL, LOW);
    batt1On = false;
    digitalWrite(BATT_2_CTL, LOW);
    batt2On = false;  
  }

  if (!inSwitchingTimeout() && armed) {

    // In the case that both batteries dropped low and then one was replaced
    if (batt1On && batt2On) {
      if (batt1V > BATT_EMPTY) {
        digitalWrite(BATT_2_CTL, LOW);
        batt2On = false;
        prevSwitchTime = micros();
        Serial.println("Selected Batt 1!");
        buffer += "Batt 1 was reinstalled charged; switching there";
      }
      if (batt2V > BATT_EMPTY) {
        digitalWrite(BATT_1_CTL, LOW);
        batt1On = false;
        prevSwitchTime = micros();
        Serial.println("Selected Batt 2!");
        buffer += "Batt 2 was reinstalled charged; switching there";
      }
    }

    // In the case that both batteries were off (disarmed)
    if (!batt1On && !batt2On) {
      if (batt1V > batt2V) {
        digitalWrite(BATT_1_CTL, HIGH);
        batt1On = true;
        buffer += "Selected batt 1 after disarm";
      }
      else {
        digitalWrite(BATT_2_CTL, HIGH);
        batt2On = true;
        buffer += "Selected batt 2 after disarm";
      }
    }

    if (batt1On && batt1V < BATT_EMPTY && batt2Installed && !batt2On) {
      ISR_Override = true;
      digitalWrite(BATT_2_CTL, HIGH);
      batt2On = true;
      prevSwitchTime = micros();
      Serial.println("Switched to batt 2!");
      buffer += "Switching to batt 2";
      if (batt2V > BATT_EMPTY) {
        digitalWrite(BATT_1_CTL, LOW);
        batt1On = false;
        prevSwitchTime = micros();
        buzzerTimer.set(BUZZ_INTERVAL_SLOW, 4);
      }
      else {
        buzzerTimer.set(BUZZ_INTERVAL_RAPID, 20);
      }
      ISR_Override = false;
    }
    if (batt2On && batt2V < BATT_EMPTY && batt1Installed && !batt1On) {
      ISR_Override = true;
      digitalWrite(BATT_1_CTL, HIGH);
      batt1On = true;
      prevSwitchTime = micros();
      Serial.println("Switched to batt 1!");
      buffer += "Switching to batt 1";
      if (batt1V > BATT_EMPTY) {
        digitalWrite(BATT_2_CTL, LOW);
        batt2On = false;
        prevSwitchTime = micros();
        buzzerTimer.set(BUZZ_INTERVAL_SLOW, 4);
      }
      else {
        buzzerTimer.set(BUZZ_INTERVAL_RAPID, 20);
      }
      ISR_Override = false;
    }

  }

  // Errors
  if (batt1Installed && batt1V < BATT_EMPTY && batt2Installed && batt2V < BATT_EMPTY) {
    if (!dualLowErrorFlag) {
      buffer += "Both batteries empty";
      dualLowErrorFlag = true;
    }
  }
  else {
    dualLowErrorFlag = false;
  }


  // Temperature
  if (MCTherm.getTemperature() > MAX_TEMP) {
    if (!MCThermErrorFlag) {
      buffer += "MC Thermometer logged: " + (String)MCTherm.getTemperature();
      MCThermErrorFlag = true;
      buzzerTimer.set(BUZZ_INTERVAL_RAPID, 20);
    }
  }
  else {
    MCThermErrorFlag = false;
  }

  if (RegTherm.getTemperature() > MAX_TEMP) {
    if (!RegThermErrorFlag) {
      buffer += "Reg Thermometer logged: " + (String)RegTherm.getTemperature();
      RegThermErrorFlag = true;
      buzzerTimer.set(BUZZ_INTERVAL_RAPID, 20);
    }
  }
  else {
    RegThermErrorFlag = false;
  }


  // Current
  if (batt1Curr > HIGH_CURRENT) {
    if (!batt1CurrErrorFlag) {
      buffer += "Batt 1 current: " + (String)getBatt1Curr();
      batt1CurrErrorFlag = true;
      buzzerTimer.set(BUZZ_INTERVAL_RAPID, 20);
    }
  }
  else {
    batt1CurrErrorFlag = false;
  }
  if (batt2Curr > HIGH_CURRENT) {
    if (!batt2CurrErrorFlag) {
      buffer += "Batt 2 current: " + (String)getBatt2Curr();
      batt2CurrErrorFlag = true;
      buzzerTimer.set(BUZZ_INTERVAL_RAPID, 20);
    }
  }
  else {
    batt1CurrErrorFlag = false;
  }


  if (buzzerTimer.timeUp()) {
    digitalWrite(BUZZER, buzzOn);
    buzzOn = !buzzOn;
  }


  if (micros() - sdPrevUpdate > 1000000) {
    if (buffer.length() > 0) {
      file.println(buffer);
      file.flush();
      buffer.remove(0);
    }
    sdPrevUpdate = micros();
  }

}

// void batt1ISR() {
//   if (batt2Installed && !inSwitchingTimeout() && !ISR_Override) {
//     digitalWrite(BATT_2_CTL, HIGH);
//     digitalWrite(BATT_1_CTL, LOW);
//     batt2On = true;
//     prevSwitchTime = micros();
//     Serial.println("Batt 1 removed, switched to 2");
//   }
//   else {
//     // fuck
//     file.println("Batt 1 disconnect while Batt2 uninstalled");
//   }
// }

// void batt2ISR() {
//   if (batt1Installed && !inSwitchingTimeout() && !ISR_Override) {
//     digitalWrite(BATT_1_CTL, HIGH);
//     digitalWrite(BATT_2_CTL, LOW);
//     batt1On = true;
//     prevSwitchTime = micros();
//     Serial.println("Batt 2 removed, switched to 1");
//   }
//   else {
//     // fuck
//     file.println("Batt 2 disconnect while Batt1 uninstalled");
//   }
// }

// Calculates the vsense pin voltage to the battery voltage
float calcVBatt(float vsense) { 
  return vsense / (2.54 / 14.94);
}

void pollBatteries() {
  batt1V = calcVBatt(3.3 * analogRead(BATT_1_VSENSE) / 1048);
  batt2V = calcVBatt(3.3 * analogRead(BATT_2_VSENSE) / 1048);

  batt1Installed = batt1V > 0.5;
  batt2Installed = batt2V > 0.5;

  if (micros() - prevCurrSense > CURR_SENSE_INTERVAL) {

    uint8_t bufferLen = sizeof(batt1VFIFO) / sizeof(float);

    float batt1Sum = 0;
    float batt2Sum = 0;
    for (int x = bufferLen - 1; x > 0; x--) {
      batt1VFIFO[x] = batt1VFIFO[x - 1];
      batt2VFIFO[x] = batt2VFIFO[x - 1];
      batt1Sum += batt1VFIFO[x];
      batt2Sum += batt2VFIFO[x];
    }
    batt1Sum += batt1V;
    batt2Sum += batt2V;
    batt1VFIFO[0] = batt1V;
    batt2VFIFO[0] = batt2V;

    float batt1VAvg = batt1Sum / bufferLen;
    float batt2VAvg = batt2Sum / bufferLen;

    // TEST LATER
    float test1Voltage = (batt1VAvg - CURR_SENSE_VOLT_OFFSET) * CURR_SENSE_VOLTAGE_MULT;
    Serial.println("ACS measured V1: " + String(test1Voltage));
    float test2Voltage = (batt2VAvg - CURR_SENSE_VOLT_OFFSET) * CURR_SENSE_VOLTAGE_MULT;
    Serial.println("ACS measured V1: " + String(test1Voltage));

    float batt1Curr = (batt1VAvg - CURR_SENSE_VOLT_OFFSET) * CURR_SENSE_CURR_MULT;
    float batt2Curr = (batt2VAvg - CURR_SENSE_VOLT_OFFSET) * CURR_SENSE_CURR_MULT;

    prevCurrSense = micros();
  }

}


bool inSwitchingTimeout() {
  return (micros() - prevSwitchTime) < SWITCHING_GRACE;
}

// float getBatt1Curr() {
//   return (((analogRead(BATT_1_CURR) / 1048) * 3.3) - 0.33) * 50;
// }

// float getBatt2Curr() {
//   return (((analogRead(BATT_2_CURR) / 1048) * 3.3) - 0.33) * 50;
// }