
#ifdef __SAMD21J18A__ 
//#erro "bla"
//#ifdef USBCON
#endif

#ifdef _VARIANT_ATMEL_SAMD21_XPRO_
#define FEATURE_LORA	1

#define	SW0				3		// switch needs pullup.
#define LED       LED_BUILTIN

#define MYSERIAL          Serial

#define LORA_SPI_MOSI   PIN_SPI_MOSI						// PA06
#define LORA_SPI_MISO   PIN_SPI_MISO						// PA04
#define LORA_SPI_SCLK   PIN_SPI_SCK							// PA07
#define LORA_CS			    PIN_SPI_SS							// PA05
#define LORA_RESET      PIN_A0								// PB00
#define LORA_DIO0       PIN_A1	// DIO0=TxDone/RXDone		// PB01
#define LORA_DIO1       PIN_A2	//							// PA10
#define LORA_DIO2       PIN_A3	// DIO2=FhssChangeChannel	// PA11
#define LORA_DIO3       PIN_A4	// DIO3=CADDone				// PA02
#define LORA_DIO4       PIN_A5	// ????						// PA03
#define LORA_DIO5       NC		// unused?

#elif __SAMD21G18A__ // Zero

#define FEATURE_LORA  1

#define SW0       1   // switch needs pullup.
#define LED       0
#define MYSERIAL    SerialUSB

#define LORA_SPI_MOSI   PIN_SPI_MOSI            // PA06
#define LORA_SPI_MISO   PIN_SPI_MISO            // PA04
#define LORA_SPI_SCLK   PIN_SPI_SCK             // PA07
#define LORA_CS         2                      // PA05
#define LORA_RESET      38                     // PB00
#define LORA_DIO0       11  // DIO0=TxDone/RXDone   // PB01
#define LORA_DIO1       13  //              // PA10
#define LORA_DIO2       10  // DIO2=FhssChangeChannel // PA11
#define LORA_DIO3       3  // DIO3=CADDone       // PA02
#define LORA_DIO4       5  // ????           // PA03
#define LORA_DIO5       NC    // unused?

#else
#error "unkown board"
#endif
