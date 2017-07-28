/*
 * The file is licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */


#ifdef ARDUINO
#include <Arduino.h>
#include "arduino-mbed.h"
#endif
#ifdef __MBED__
#include "mbed.h"
#include "PinMap.h"
#endif
#include "RadioStatusInterface.h"
#include "RadioStatus.h"

#ifdef FEATURE_LORA


STMRadioStatus::STMRadioStatus()
{
    ledTX = NULL;
    ledRX = NULL;
    ledTimeout = NULL;
    
#ifdef TARGET_STM32L0
    ledTX = new DigitalOut(LED3); // blue
    *ledTX = 0;
    ledRX = new DigitalOut(LED4); // red
    *ledRX = 0;
    ledTimeout = new DigitalOut(LED2); // green
    *ledTimeout = 0;
#endif
}

void
STMRadioStatus::TXStart(int AppID, int toStation, int length)
{
    if (ledTX)
    	*ledTX = 1;
}

void
STMRadioStatus::TXComplete(void)
{
    if (ledTX)
		*ledTX = 0;
}

void
STMRadioStatus::RxDone(int size, int rssi, int snr)
{
    if (ledRX)
	    *ledRX = 1;
}

void
STMRadioStatus::RxCompleted(void)
{
    if (ledRX)
    	*ledRX = 0;
}

void
STMRadioStatus::MessageTimeout(int App, int toStation)
{
    if (ledTimeout)
    	*ledTimeout = 1;
}

#endif // FEATURE_LORA
