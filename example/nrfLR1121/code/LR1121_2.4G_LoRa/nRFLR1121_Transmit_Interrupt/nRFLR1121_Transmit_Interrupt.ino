/*
  RadioLib LR11x0 Transmit with Interrupts Example

  This example transmits LoRa packets with one second delays
  between them. Each packet contains up to 256 bytes
  of data, in the form of:
  - Arduino String
  - null-terminated char array (C-string)
  - arbitrary binary data (byte array)

  Other modules from LR11x0 family can also be used.

  This example assumes Seeed Studio Wio WM1110 is used.
  For other LR11x0 modules, some configuration such as
  RF switch control may have to be adjusted.

  For default module settings, see the wiki page
  https://github.com/jgromes/RadioLib/wiki/Default-configuration#lr11x0---lora-modem

  For full API reference, see the GitHub Pages
  https://jgromes.github.io/RadioLib/
*/

// include the library
#include <RadioLib.h>

// LR1121 has the following connections:
// NSS pin:   44
// IRQ pin:   40
// NRST pin:  42
// BUSY pin:  43
LR1121 radio = new Module(44, 40, 42, 43);

// or detect the pinout automatically using RadioBoards
// https://github.com/radiolib-org/RadioBoards
/*
  #define RADIO_BOARD_AUTO
  #include <RadioBoards.h>
  Radio radio = new RadioModule();
*/

// set RF switch configuration for Wio WM1110
// Wio WM1110 uses DIO5 and DIO6 for RF switching
// NOTE: other boards may be different!
static const uint32_t rfswitch_dio_pins[] = {
  RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
  RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
  // mode                  DIO5  DIO6
  { LR11x0::MODE_STBY, { LOW, LOW } },
  { LR11x0::MODE_RX, { HIGH, LOW } },
  { LR11x0::MODE_TX, { LOW, HIGH } },
  { LR11x0::MODE_TX_HP, { LOW, HIGH } },
  { LR11x0::MODE_TX_HF, { LOW, LOW } },
  { LR11x0::MODE_GNSS, { LOW, LOW } },
  { LR11x0::MODE_WIFI, { LOW, LOW } },
  END_OF_MODE_TABLE,
};

// save transmission state between loops
int transmissionState = RADIOLIB_ERR_NONE;

void setup() {
  Serial.begin(115200);
  // while (!Serial)
  //   ;           // Wait for serial to be initialised
  // delay(1000);  // Give time to switch to the serial monitor

  SPI.setPins(47, 45, 46);
  SPI.begin();

  // initialize LR1110 with default settings
  Serial.print(F("[LR1121] Initializing ... "));
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) {
      delay(10);
    }
  }

  // set the function that will be called
  // when packet transmission is finished
  radio.setPacketSentAction(setFlag);

  /*
        Sets carrier frequency.
        SX1278/SX1276 : Allowed values range from 137.0 MHz to 525.0 MHz.
        SX1268/SX1262 : Allowed values are in range from 150.0 to 960.0 MHz.
        SX1280        : Allowed values are in range from 2400.0 to 2500.0 MHz.
        LR1121        : Allowed values are in range from 150.0 to 960.0 MHz, 1900 - 2200 MHz and 2400 - 2500 MHz. Will also perform calibrations.
    * * * */

  if (radio.setFrequency(2400.5) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    //  if (radio.setFrequency(915.0) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    Serial.println(F("Selected frequency is invalid for this module!"));
    while (true)
      ;
  }

  /*
        Sets LoRa link bandwidth.
        SX1278/SX1276 : Allowed values are 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250 and 500 kHz. Only available in %LoRa mode.
        SX1268/SX1262 : Allowed values are 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0 and 500.0 kHz.
        SX1280        : Allowed values are 203.125, 406.25, 812.5 and 1625.0 kHz.
        LR1121        : Allowed values are 62.5, 125.0, 250.0 and 500.0 kHz.
    * * * */
  if (radio.setBandwidth(203.125, true) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
    Serial.println(F("Selected bandwidth is invalid for this module!"));
    while (true)
      ;
  }


  /*
      Sets LoRa link spreading factor.
      SX1278/SX1276 :  Allowed values range from 6 to 12. Only available in LoRa mode.
      SX1262        :  Allowed values range from 5 to 12.
      SX1280        :  Allowed values range from 5 to 12.
      LR1121        :  Allowed values range from 5 to 12.
    * * * */
  if (radio.setSpreadingFactor(10) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
    Serial.println(F("Selected spreading factor is invalid for this module!"));
    while (true)
      ;
  }

  /*
      Sets LoRa coding rate denominator.
      SX1278/SX1276/SX1268/SX1262 : Allowed values range from 5 to 8. Only available in LoRa mode.
      SX1280        :  Allowed values range from 5 to 8.
      LR1121        :  Allowed values range from 5 to 8.
    * * * */
  if (radio.setCodingRate(6) == RADIOLIB_ERR_INVALID_CODING_RATE) {
    Serial.println(F("Selected coding rate is invalid for this module!"));
    while (true)
      ;
  }

  /*
      Sets LoRa sync word.
      SX1278/SX1276/SX1268/SX1262/SX1280 : Sets LoRa sync word. Only available in LoRa mode.
    * * */
  if (radio.setSyncWord(0x12) != RADIOLIB_ERR_NONE) {
    Serial.println(F("Unable to set sync word!"));
    while (true)
      ;
  }

  /*
      Sets transmission output power.
      SX1278/SX1276 :  Allowed values range from -3 to 15 dBm (RFO pin) or +2 to +17 dBm (PA_BOOST pin). High power +20 dBm operation is also supported, on the PA_BOOST pin. Defaults to PA_BOOST.
      SX1262        :  Allowed values are in range from -9 to 22 dBm. This method is virtual to allow override from the SX1261 class.
      SX1268        :  Allowed values are in range from -9 to 22 dBm.
      SX1280        :  Allowed values are in range from -18 to 13 dBm. PA Version range : -18 ~ 3dBm
      LR1121        :  Allowed values are in range from -17 to 22 dBm (high-power PA) or -18 to 13 dBm (High-frequency PA)
    * * * */
  if (radio.setOutputPower(13) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
    Serial.println(F("Selected output power is invalid for this module!"));
    while (true)
      ;
  }

  /*
      Sets preamble length for LoRa or FSK modem.
      SX1278/SX1276 : Allowed values range from 6 to 65535 in %LoRa mode or 0 to 65535 in FSK mode.
      SX1262/SX1268 : Allowed values range from 1 to 65535.
      SX1280        : Allowed values range from 1 to 65535. preamble length is multiple of 4
      LR1121        : Allowed values range from 1 to 65535.
    * * */
  if (radio.setPreambleLength(16) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH) {
    Serial.println(F("Selected preamble length is invalid for this module!"));
    while (true)
      ;
  }

  // Enables or disables CRC check of received packets.
  if (radio.setCRC(false) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) {
    Serial.println(F("Selected CRC is invalid for this module!"));
    while (true)
      ;
  }

  radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

  // LR1121 TCXO Voltage 2.85~3.15V
  radio.setTCXO(3.3);

  // start transmitting the first packet
  Serial.print(F("[LR1121] Sending first packet ... "));

  // you can transmit C-string or Arduino string up to
  // 256 characters long
  transmissionState = radio.startTransmit("Hello World!");

  // you can also transmit byte array up to 256 bytes long
  /*
    byte byteArr[] = {0x01, 0x23, 0x45, 0x67,
                      0x89, 0xAB, 0xCD, 0xEF};
    state = radio.startTransmit(byteArr, 8);
  */

  // radio.transmitDirect(0);
}

// flag to indicate that a packet was sent
volatile bool transmittedFlag = false;

// this function is called when a complete packet
// is transmitted by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // we sent a packet, set the flag
  transmittedFlag = true;
}

// counter to keep track of transmitted packets
int count = 0;

void loop() {
  // check if the previous transmission finished
  if (transmittedFlag) {
    // reset flag
    transmittedFlag = false;

    if (transmissionState == RADIOLIB_ERR_NONE) {
      // packet was successfully sent
      Serial.println(F("transmission finished!"));

      // NOTE: when using interrupt-driven transmit method,
      //       it is not possible to automatically measure
      //       transmission data rate using getDataRate()

    } else {
      Serial.print(F("failed, code "));
      Serial.println(transmissionState);
    }

    // clean up after transmission is finished
    // this will ensure transmitter is disabled,
    // RF switch is powered down etc.
    radio.finishTransmit();

    // wait a second before transmitting again
    delay(10);

    // send another one
    Serial.print(F("[LR1121] Sending another packet ... "));

    // you can transmit C-string or Arduino string up to
    // 256 characters long
    String str = "Hello World! #" + String(count++);
    transmissionState = radio.startTransmit(str);

    // you can also transmit byte array up to 256 bytes long
    /*
      byte byteArr[] = {0x01, 0x23, 0x45, 0x67,
                        0x89, 0xAB, 0xCD, 0xEF};
      transmissionState = radio.startTransmit(byteArr, 8);
    */
  }
}
