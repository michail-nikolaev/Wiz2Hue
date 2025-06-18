#include <Arduino.h>

void ledDigital(int *left, int period, int pin, int sleep)
{
  *left -= sleep;
  if (*left < period)
  {
    digitalWrite(pin, LOW);
  }
  else
  {
    digitalWrite(pin, HIGH);
  }
  if (*left < 0)
  {
    *left = period * 2;
  }
}

void ledAnalog(int *left, int period, int pin, int sleep)
{
  *left -= sleep;
  float brigtness = (float)abs(*left) / period;
  analogWrite(pin, brigtness * 255);
  if (*left < -period)
  {
    *left = period;
  }
}
