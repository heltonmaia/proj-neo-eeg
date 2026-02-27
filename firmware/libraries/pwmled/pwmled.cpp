#include  "Arduino.h"
#include  "pwmled.h"

pwmled::pwmled(int bright_val)
{
  bright=bright_val;
  bright=-1;
  bright_low=0;
  bright_high=bright_val;
  period=100;
  counter=100;
  changed=false;
}
