#ifndef BUZZER_TIMER
#define BUZZER_TIMER

#include <Arduino.h>

class BuzzerTimer {
  public:
    BuzzerTimer(void);
    void set(unsigned long interval, unsigned long repetitionsRequired, bool onOff=true);
    bool timeUp(void);
  private:
    unsigned long _prevTime;
    unsigned long _interval;
    unsigned long _repetitionsRequired;
    unsigned long _repetitionsLive;
    unsigned long _intervalMetFlag;
    bool _finished;
};

#endif