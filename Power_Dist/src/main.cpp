#include <Arduino.h>

#include <SPI.h>
#include <SD.h>

#include <Adafruit_PCT2075.h>

Adafruit_PCT2075 MCTherm;
Adafruit_PCT2075 RegTherm;

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

File file;
String buffer;

void updateSD();

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

}

void loop() {

  // MCTherm.getTemperature();
  // RegTherm.getTemperature();

  updateSD();
}


// Idea is the log everything to the buffer variable and then to `updateSD()` at the end of the loop to save time
void updateSD(void) {
  if (buffer.length() > 0) {
    file.println(buffer);
  }
  buffer.remove(0);
}