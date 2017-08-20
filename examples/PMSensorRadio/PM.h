
class PMSensor {
  public:
    bool SensorInit(Uart *serial, int baud = 9600);
    bool ReadRecord(void);
    uint16_t getPM10(void);
    uint16_t getPM25(void);
    uint16_t getPMid(void);

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
};
