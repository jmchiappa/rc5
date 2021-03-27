/*
  rc5.cpp - rc5 library v1.0.0 - 2021-03-27
  Copyright (c) 2021 Jean-Marc Chiappa.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  See file LICENSE.txt for further informations on licensing terms.
*/

// RC5 Protocol decoder Arduino code

#include "rc5.h"

/**
  Debug helper
*/

//#define DEBUG

#ifdef DEBUG

  typedef struct elm {
    uint16_t time;
    uint8_t rc5_state;
    uint8_t nb_j;
    uint8_t pinstate;
    char comment[10];
  };  

  elm buffer[1024];
  uint32_t ptrpusher=0;
  uint32_t ptrpuller=0;

  #define Debug_print(a,b)      {Serial.print(a);Serial.print(" : ");Serial.print(b);}
  #define Debug_println(a,b)    {Serial.print(a);Serial.print(" : ");Serial.println(b);}

  inline void pushtrace(uint16_t time,uint8_t state,uint8_t j,uint8_t pin, char *comment){
      buffer[ptrpusher] = {           
        .time = time,                   
        .rc5_state = state,             
        .nb_j = j,                      
        .pinstate = digitalRead(PIN_RECEIVER)
      };
      strcpy(buffer[ptrpusher].comment,comment);
      ptrpusher++;
      ptrpusher = ptrpusher % 1024;     
  }

  inline void poptrace(void) {
    if(ptrpusher!=ptrpuller) {
      Debug_print("time", buffer[ptrpuller].time);
      Debug_print("\t", buffer[ptrpuller].comment);
      Debug_print("\trc5 state", buffer[ptrpuller].rc5_state);
      Debug_print("\tj", buffer[ptrpuller].nb_j);
      Debug_println("\tpin state", buffer[ptrpuller].pinstate);
      ptrpuller++;
      ptrpuller = ptrpuller % 1024;
    }
  }

#else

  #define Debug_print(a,b)      {}
  #define Debug_println(a,b)    {}
  inline void pushtrace(uint16_t time,uint8_t state,uint8_t j,uint8_t pin, char *comment){}
  inline void poptrace()        {}

#endif

/**
 Local variables shared with ISR
*/
  
uint8_t j;
HardwareTimer *tim;
boolean rc5_ok;
uint32_t timer_value;
uint8_t rc5_state=0;
uint16_t rc5_code;

rc5::rc5(uint8_t pin_receiver, TIM_TypeDef *instance){
  pin = pin_receiver;
  htimer = instance;
};


void rc5::begin() {
  if(htimer==NULL) {
    htimer = TIM6;
  }
  tim = new HardwareTimer(htimer);
  tim->setPrescaleFactor(4);
  tim->setOverflow(3000,MICROSEC_FORMAT); 
  tim->pause();
  tim->setCount(0);
  tim->attachInterrupt(overflow);
  attachInterrupt(pin, rc5_read, CHANGE);          // Enable external interrupt (INT0)
}

static void rc5_read() {
  if(!rc5_ok){
    if(rc5_state != 0){
      timer_value = tim->getCount(MICROSEC_FORMAT);                         // Store Timer1 value
      tim->setCount(0);
      pushtrace(timer_value,rc5_state,j,0xFF,"->RC5 Read");
    }
    switch(rc5_state){
     case 0 :
        tim->setCount(0);
        j = 0;
        pushtrace(tim->getCount(),rc5_state,j,0xFF,"start");
        tim->resume();
        rc5_state = 1;                               // Next state: end of mid1
        rc5_code=0;
        break;
     case 1 :                                      // End of mid1 ==> check if we're at the beginning of start1 or mid0
      if((timer_value > long_time) || (timer_value < short_time)){         // Invalid interval ==> stop decoding and reset
        rc5_state = 0;                             // Reset decoding process
        tim->pause();
        pushtrace(timer_value,rc5_state,j,0xFF,"Err-mid1");
        break;
      }
      pushtrace(timer_value,rc5_state,j,1,"End-mid1");
      bitSet(rc5_code, 13 - j);
      j++;
      if(j > 13){                                  // If all bits are received
        rc5_ok = 1;                                // Decoding process is OK
        break;
      }
      if(timer_value > med_time){                // We're at the beginning of mid0
        rc5_state = 2;                           // Next state: end of mid0
        if(j == 13){                             // If we're at the LSB bit
          rc5_ok = 1;                            // Decoding process is OK
          pushtrace(timer_value,rc5_state,j,0,"complete");
          bitClear(rc5_code, 0);                 // Clear the LSB bit
          break;
        }
      } else
        rc5_state = 3;                           // Next state: end of start1

      break;
     case 2 :                                      // End of mid0 ==> check if we're at the beginning of start0 or mid1
      if((timer_value > long_time) || (timer_value < short_time)){
        rc5_state = 0;                             // Reset decoding process
        tim->pause();
        pushtrace(timer_value,rc5_state,j,0xFF,"Err-mid0");
        break;
      }
      pushtrace(timer_value,rc5_state,j,0,"End-mid0");
      bitClear(rc5_code, 13 - j);
      j++;
      if(timer_value > med_time)                   // We're at the beginning of mid1
        rc5_state = 1;                             // Next state: end of mid1
      else                                         // We're at the beginning of start0
        rc5_state = 4;                             // Next state: end of start0

      break;
     case 3 :                                      // End of start1 ==> check if we're at the beginning of mid1
      if((timer_value > med_time) || (timer_value < short_time)){           // Time interval invalid ==> stop decoding
        pushtrace(timer_value,rc5_state,j,0xFF,"Err-start1");
        tim->pause();
        rc5_state = 0;                             // Reset decoding process
        break;
      }
      pushtrace(timer_value,rc5_state,j,0x80,"end-start1");
      rc5_state = 1;                             // Next state: end of mid1
      break;
     case 4 :                                      // End of start0 ==> check if we're at the beginning of mid0
      if((timer_value > med_time) || (timer_value < short_time)){           // Time interval invalid ==> stop decoding
        pushtrace(timer_value,rc5_state,j,0xFF,"Err-start0");
        tim->pause();
        rc5_state = 0;                             // Reset decoding process
        break;
      }
      rc5_state = 2;                             // Next state: end of mid0
      pushtrace(timer_value,rc5_state,j,0x40,"End-mid0");
      if(j == 13){                                 // If we're at the LSB bit
        rc5_ok = 1;                                // Decoding process is OK
        pushtrace(timer_value,rc5_state,j,0,"complete");
        bitClear(rc5_code, 0);                     // Clear the LSB bit
      }
    }
  }
}

static void overflow() {
  pushtrace(tim->getCount(MICROSEC_FORMAT),0,0,0xFF,"overflow");  
  rc5_state = 0;                                 // Reset decoding process
  tim->pause();
} 
 

boolean rc5::codeReceived() {
  return rc5_ok!=0;
}

boolean rc5::newKeyPressed() {
  update();
  return previous_toggleBit != toggleBit;
}

uint16_t rc5::rawCode() {
  update();
  rc5_ok = 0;
  return rc5_code;
}

uint8_t rc5::Command() {
  update();
  rc5_ok = 0;
  return command;
}

uint8_t rc5::Address() {
  update();
  return address;
};

void rc5::skipThisCode(){
  tim->pause();
  rc5_ok=0;
  rc5_state = 0;
  previous_toggleBit = toggleBit;
};

void rc5::update() {
  poptrace();
  if(rc5_ok){                                    // If the mcu receives RC5 message with successful
    previous_toggleBit = toggleBit;
    rc5_state = 0;
    tim->pause();
    toggleBit = bitRead(rc5_code, 11);          // Toggle bit is bit number 11
    address = (rc5_code >> 6) & 0x1F;            // Next 5 bits are for address
    command = rc5_code & 0x3F;                   // The 6 LSBits are command bits
  }
}
