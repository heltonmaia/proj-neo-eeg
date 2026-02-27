/*
  pwmled.h - Library for flashing led.
  George Nascimento
*/
#ifndef pwmled_h
#define pwmled_h

#include "Arduino.h"

class pwmled
{
  public:
    pwmled(int bright_val);
    boolean state;
    boolean changed;
    int bright_low;
    int bright_high;
    int bright;
    int period;
    int counter;
};

#endif
