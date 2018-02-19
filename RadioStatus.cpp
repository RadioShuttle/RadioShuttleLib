/*
 * The file is licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */


#ifdef ARDUINO
#include <Arduino.h>
#include "arduino-mbed.h"
#include <time.h>
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
    _totalTX = 0;
    _totalRX = 0;
    _totalError = 0;
    _totalTimeout = 0;

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
#ifdef MyHOME_BOARD_REV4
    ledTX = new DigitalOut(LED2); // yellow
    *ledTX = 0;
    ledRX = new DigitalOut(STATUS_LED); // red
    *ledRX = 0;
    ledTimeout = new DigitalOut(LED3); // green
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
#ifdef ARDUINO_ESP32_DEV // ESP32_ECO_POWER_REV_1
    ledTX = new DigitalOut(2); 	// green
    *ledTX = 0;
    ledRX = new DigitalOut(12);	// red
    *ledRX = 0;
#endif
#ifdef ARDUINO_Heltec_WIFI_LoRa_32
    invertedDisplay = false;
    _line1[0] = 0;
    _line2[0] = 0;
    _line3[0] = 0;
    _line4[0] = 0;
    _line5[0] = 0;
    
#define DISPLAY_ADDRESS 0x3c
#define DISPLAY_SDA     4
#define DISPLAY_SCL     15
#define DISPLAY_RESET   16    
    displayReset = new DigitalOut(DISPLAY_RESET);
    display = new SSD1306(DISPLAY_ADDRESS, DISPLAY_SDA, DISPLAY_SCL);
    *displayReset = 0;
    wait_ms(50);
    *displayReset = 1;
    display->init();
    // display->flipScreenVertically();
    display->setFont(ArialMT_Plain_16); // ArialMT_Plain_10);
    display->clear();
    display->drawString(0, 0, "RadioShuttle 1.4");
    display->setFont(ArialMT_Plain_10);
    int yoff = 17;
    display->drawString(0, yoff, "Peer-to-Peer LoRa Protcol");
    yoff += 12;
    display->drawString(0, yoff, "Efficient, Fast, Secure");
    yoff += 12;
    display->drawString(0, yoff, "www.radioshuttle.de");
    display->display();
#endif
}

MyRadioStatus::~MyRadioStatus()
{
    if (ledTX) {
        if (inverted)
            *ledTX = 1;
        else
            *ledTX = 0;
        delete ledTX;
    }
    if (ledRX) {
        if (inverted)
            *ledRX = 1;
        else
            *ledRX = 0;
        delete ledRX;
    }
    if (ledTimeout) {
        if (inverted)
            *ledTimeout = 1;
        else
            *ledTimeout = 0;
        delete ledTimeout;
    }
}

void
MyRadioStatus::TXStart(int AppID, int toStation, int length, int dBm)
{
    if (ledTX) {
    	if (inverted)
            *ledTX = 0;
        else
        	*ledTX = 1;
    }
#ifdef ARDUINO_Heltec_WIFI_LoRa_32
    snprintf(_line2, sizeof(_line2), "TX(%d) ID(%d) %d dBm", length, toStation, dBm);
    UpdateDisplay(true);
#endif
    _totalTX++;
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
#ifdef ARDUINO_Heltec_WIFI_LoRa_32
	UpdateDisplay(false);
#endif
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
    _totalRX++;
#ifdef ARDUINO_Heltec_WIFI_LoRa_32
    snprintf(_line3, sizeof(_line3), "RX(%d) RSSI(%d) SNR(%d)", size, rssi, snr);
    UpdateDisplay(true);
#endif
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
#ifdef ARDUINO_Heltec_WIFI_LoRa_32
    UpdateDisplay(false);
#endif
}

void
MyRadioStatus::MessageTimeout(int App, int toStation)
{
    if (ledTimeout)
    	*ledTimeout = 1;
    _totalTimeout++;
#ifdef ARDUINO_Heltec_WIFI_LoRa_32
    UpdateDisplay(false);
#endif
}


void
MyRadioStatus::UpdateDisplay(bool invertDisplay)
{
#ifdef ARDUINO_Heltec_WIFI_LoRa_32
    int yoff = 0;
    int hight = 12;
    time_t t = time(NULL);
    struct tm mytm;
    localtime_r(&t, &mytm);

    snprintf(_line1, sizeof(_line1), "%s (%d) %02d:%02d:%02d", _radioType, _stationID,
             mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
    snprintf(_line4, sizeof(_line4), "Packets RX(%d) TX(%d)", _totalRX, _totalTX);
    snprintf(_line5, sizeof(_line5), "RXErr(%d) TOut(%d) %.2f %d", _totalError, _totalTimeout, (double)_frequency/1000000.0, _spreadingFactor);
#if 1
    if (invertDisplay)
        display->invertDisplay();
    else
        display->normalDisplay();
#endif
    display->setFont(ArialMT_Plain_10);
    display->clear();

    display->drawString(0, yoff, String(_line1));
    yoff += hight;
    display->drawString(0, yoff, String(_line2));
    yoff += hight;
    display->drawString(0, yoff, String(_line3));
    yoff += hight;
    display->drawString(0, yoff, String(_line4));
    yoff += hight;
    display->drawString(0, yoff, String(_line5));
    yoff += hight;
    
    display->display();
#endif
}

#endif // FEATURE_LORA
