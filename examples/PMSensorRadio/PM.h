/*
   The file is Licensed under the Apache License, Version 2.0
   (c) 2017 Helmut Tschemernjak
   30826 Garbsen (Hannover) Germany
*/

class PMSensor {
  public:
    bool SensorInit(Uart *serial, int baud = 9600);
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
    Uart *_ser;
    const static int PM_WARMUP_SECONDS = 20;
};
