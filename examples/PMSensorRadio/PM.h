/*
   The file is Licensed under the Apache License, Version 2.0
   (c) 2017 Helmut Tschemernjak
   30826 Garbsen (Hannover) Germany
*/

#ifdef ARDUINO_ARCH_ESP32
#define SERIALTYPE  HardwareSerial
#define PMSerial    Serial
#elif defined(D21_LONGRA_REV_720) || defined(D21_LONGRA_REV_750)
#define SERIALTYPE  Uart
#define PMSerial    Serial1
#else
#error "Unkown Board"
#endif

class PMSensor {
  public:
    bool SensorInit(SERIALTYPE *serial, int baud = 9600);
    void EnablePower(void);
    void DisablePower(void);
    bool ReadRecord(void);
    uint16_t getPM10(void);
    uint16_t getPM25(void);
    uint16_t getPMid(void);
    int getWarmUpSeconds(void) {
      return PM_WARMUP_SECONDS;
    };

private:
    bool PMChecksumOK();
    enum PMTypes {
      PM_MSG_HEADER = 0xaa,
      PM_MSG_TAIL = 0xab,
    };

    struct SensorRec {
      uint8_t header;
      uint8_t cmd;
      uint16_t pm25;
      uint16_t pm10;
      uint16_t id;
      uint8_t sum;
      uint8_t tail;
    };

private:
    SensorRec _rec;
    SensorRec _data;
    Stream *_ser;
    const static int PM_WARMUP_SECONDS = 20;
};
