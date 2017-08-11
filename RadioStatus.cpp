/*
 * The file is licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */


#ifdef ARDUINO
#include <Arduino.h>
#include "arduino-mbed.h"
#define FEATURE_LORA    1
#endif
#ifdef __MBED__
#include "mbed.h"
#include "PinMap.h"
#endif
#include "RadioStatusInterface.h"
#include "RadioStatus.h"

#ifdef FEATURE_LORA


MyRadioStatus::MyRadioStatus()
{
    ledTX = NULL;
    ledRX = NULL;
    ledTimeout = NULL;
    inverted = false;
    
#ifdef TARGET_STM32L0
    ledTX = new DigitalOut(LED3); // blue
    *ledTX = 0;
    ledRX = new DigitalOut(LED4); // red
    *ledRX = 0;
    ledTimeout = new DigitalOut(LED1); // green
    *ledTimeout = 0;
#endif
#ifdef __SAMD21G18A__
    inverted = true;
#ifdef PIN_LED_TXL
    ledTX = new DigitalOut(PIN_LED_TXL); // yellow
    *ledTX = 1;
#endif
#ifdef PIN_LED_RXL
    ledRX = new DigitalOut(25); // red
    *ledRX = 1;
#endif
#endif
}

void
MyRadioStatus::TXStart(int AppID, int toStation, int length)
{
    if (ledTX) {
    	if (inverted)
            *ledTX = 0;
        else
        	*ledTX = 1;
    }
}

void
MyRadioStatus::TXComplete(void)
{
    if (ledTX) {
        if (inverted)
            *ledTX = 1;
		else
			*ledTX = 0;
    }
}

void
MyRadioStatus::RxDone(int size, int rssi, int snr)
{
    if (ledRX) {
        if (inverted)
            *ledRX = 0;
		else
            *ledRX = 1;
    }
}

void
MyRadioStatus::RxCompleted(void)
{
    if (ledRX) {
        if (inverted)
            *ledRX = 1;
        else
        	*ledRX = 0;
    }
}

void
MyRadioStatus::MessageTimeout(int App, int toStation)
{
    if (ledTimeout)
    	*ledTimeout = 1;
}

#endif // FEATURE_LORA
