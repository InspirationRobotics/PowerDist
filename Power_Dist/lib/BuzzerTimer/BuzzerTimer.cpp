#include "BuzzerTimer.h"

BuzzerTimer::BuzzerTimer() {
  _finished = true;
};

void BuzzerTimer::set(unsigned long interval, unsigned long repetitionsRequired, bool onOff) {
  if (!_finished)
    return;
  _interval = interval;
  _repetitionsRequired = repetitionsRequired * (onOff ? 2 : 1);
  _repetitionsLive = 0;
  _intervalMetFlag = false;
  _prevTime = micros();
  _finished = false;
}

bool BuzzerTimer::timeUp() {
  if ((micros() - _prevTime) > _interval && !_intervalMetFlag && _repetitionsLive < _repetitionsRequired && !_finished) {
    _prevTime = micros();
    _intervalMetFlag = true;
    _repetitionsLive++;
      if (_repetitionsLive == _repetitionsRequired)
        _finished = true;
    return true;
  }
  else {
    _intervalMetFlag = false;
  }
  return false;
}