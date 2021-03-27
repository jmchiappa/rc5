/*
  rc5.h - rc5 library v1.0.0 - 2021-03-27
  Copyright (c) 2021 Jean-Marc Chiappa.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  See file LICENSE.txt for further informations on licensing terms.
*/

// RC5 Protocol decoder Arduino code
#ifndef rc5_h
#define rc5_h

#include "Arduino.h"
 
// Define number of Timer1 ticks (with a prescaler of 1/8)
#define short_time     450                      // Used as a minimum time for short pulse or short space ( ==>  700 us)
#define   med_time     1300                      // Used as a maximum time for short pulse or short space ( ==> 1200 us)
#define  long_time     3000                      // Used as a maximum time for long pulse or long space   ( ==> 2000 us)

static void rc5_read();
static void overflow();

class rc5
{
public:
  rc5(uint8_t pin_receiver, TIM_TypeDef *instance);
  void begin();
  boolean codeReceived();
  boolean newKeyPressed();
  uint16_t rawCode();
  uint8_t Command();
  uint8_t Address();
  void skipThisCode();
private:
  void update(void);
  TIM_TypeDef *htimer=NULL;
  uint8_t pin;
  uint8_t previous_toggleBit;
  uint8_t toggleBit=0xFF;
  uint8_t address;
  uint8_t command;
};

#endif // rc5_h