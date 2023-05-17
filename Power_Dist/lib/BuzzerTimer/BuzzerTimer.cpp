#include "BuzzerTimer.h"

BuzzerTimer::BuzzerTimer(){};

void BuzzerTimer::set(unsigned long interval, unsigned long repetitionsRequired, bool onOff) {
  _interval = interval;
  _repetitionsRequired = repetitionsRequired * (onOff ? 2 : 1);
  _repetitionsLive = 0;
  _intervalMetFlag = false;
  _prevTime = micros();
}
bool BuzzerTimer::timeUp() {
  if ((micros() - _prevTime) > _interval && !_intervalMetFlag && _repetitionsLive < _repetitionsRequired) {
    _prevTime = micros();
    _intervalMetFlag = true;
    _repetitionsLive++;
    return true;
  }
  else {
    _intervalMetFlag = false;
  }
  return false;
}