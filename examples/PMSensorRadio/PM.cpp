/*
   The file is Licensed under the Apache License, Version 2.0
   (c) 2017 Helmut Tschemernjak
   30826 Garbsen (Hannover) Germany
*/
#ifdef ARDUINO

#include "PinMap.h"
#include <Arduino.h>
#include <arduino-mbed.h>
#include <arduino-util.h>

#ifdef FEATURE_LORA

#include "PM.h"

#ifdef BOOSTER_EN50
extern DigitalOut boost50;
#endif

bool
PMSensor::SensorInit(SERIALTYPE *serial, int baud)
{
  serial->begin(baud);
  _ser = serial;
  return true;
}

void
PMSensor::EnablePower(void)
{
#ifdef BOOSTER_EN50
  boost50 = 1;
#endif  
}

void
PMSensor::DisablePower(void)
{
#ifdef BOOSTER_EN50
  boost50 = 0;
#endif  
}


bool
PMSensor::ReadRecord(void)
{
  unsigned char lastchar = PM_MSG_TAIL;

  while (_ser->available()) {
    delay(3);  //delay to allow buffer to fill

    if (_ser->available() > 0) {
      unsigned char c = _ser->read();
      if (lastchar == PM_MSG_TAIL && c == PM_MSG_HEADER) {
        _rec.header = c;
        int rlen = sizeof(_rec) - 1;
        int cnt = _ser->readBytes(&_rec.cmd, rlen);
        if (cnt != rlen) {
          dprintf("Invalid read length: %d\n", cnt);
          return false;
        }
        if (!PMChecksumOK()) {
          dump("InvalidSensorData", &_rec, sizeof(_rec));
          dprintf("Invalid checksum");
          return false;
        }
        _data = _rec;
        // dump("FinalSensorData", &_rec, sizeof(_rec));
        dprintf("ParticalData: PM10: %.1f (μg/m3)   PM2.5: %.1f (μg/m3)    ID: %d", (float)_rec.pm10 / 10.0, (float)_rec.pm25 / 10.0, _rec.id);
        return true;
      }
    }
  }
  return false;
}

bool
PMSensor::PMChecksumOK()
{
  // verify checksum
  uint8_t sum = 0;
  unsigned char *p = (unsigned char *)&_rec.pm25;

  for (int i = 0; i < 6; i++) // data1-6
    sum += *p++;
  if (sum != _rec.sum) {
    return true;
  }
  return false;
}

uint16_t
PMSensor::getPM10(void)
{
  return _data.pm10;
}


uint16_t
PMSensor::getPM25(void)
{
  return _data.pm25;
}


uint16_t
PMSensor::getPMid(void)
{
  return _data.id;
}

#endif // FEATURE_LORA
#endif // ARDUINO

