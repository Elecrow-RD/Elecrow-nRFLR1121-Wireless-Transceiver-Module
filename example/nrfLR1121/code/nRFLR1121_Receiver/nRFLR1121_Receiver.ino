
// include the library
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>  // for Serial
#include <RadioLib.h>

LR1121 radio = new Module(44, 40, 42, 43);
static const uint32_t rfswitch_dio_pins[] = {
  RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
  RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
  // mode                  DIO5  DIO6
  { LR11x0::MODE_STBY, { LOW, LOW } },
  { LR11x0::MODE_RX, { HIGH, LOW } },
  { LR11x0::MODE_TX, { HIGH, HIGH } },
  { LR11x0::MODE_TX_HP, { LOW, HIGH } },
  { LR11x0::MODE_TX_HF, { LOW, LOW } },
  { LR11x0::MODE_GNSS, { LOW, LOW } },
  { LR11x0::MODE_WIFI, { LOW, LOW } },
  END_OF_MODE_TABLE,
};

void setup() {
  Serial.begin(115200);
  pinMode(41, OUTPUT);
  SPI.setPins(47, 45, 46);
  SPI.begin();
  // int state = radio.begin(915.0, 125.0, 12, 7, RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE, 22, 8, 3.3);
  int state = radio.begin(915.0, 125.0, 10, 7, RADIOLIB_LR11X0_LORA_SYNC_WORD_PUBLIC, 22, 8, 3.3);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) {
      delay(10000);
    }
  }
  radio.setRxBoostedGainMode(true);
  // set RF switch control configuration
  radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

  // set the function that will be called
  // when new packet is received
  radio.setPacketReceivedAction(setFlag);

  // start listening for LoRa packets
  Serial.print(F("[LR1121] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) {
      delay(10000);
    }
  }

}

// flag to indicate that a packet was received
volatile bool receivedFlag = false;
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // we got a packet, set the flag
  receivedFlag = true;
}

// Function to parse sensor data
void parseSensorData(String rawData) {
  // Example: Temp:25.5,Hum:60.0,X:0.12,Y:-0.03,Z:9.81
  int start = 0;
  while (start < rawData.length()) {
    int end = rawData.indexOf(',', start);
    if (end == -1) end = rawData.length();
    
    String pair = rawData.substring(start, end);
    int colon = pair.indexOf(':');
    if (colon != -1) {
      String key = pair.substring(0, colon);
      String value = pair.substring(colon + 1);
      
      Serial.print(key);
      Serial.print(": ");
      Serial.print(value);
      if (key == "Temp") Serial.print(" °C");
      else if (key == "Hum") Serial.print(" %");
      else if (key.startsWith("X") || key.startsWith("Y") || key.startsWith("Z")) Serial.print(" m/s²");
      Serial.println();
    }
    start = end + 1;
  }
}

void loop() {
  // check if the flag is set
  if (receivedFlag) {
    // reset flag
    receivedFlag = false;

    // you can read received data as an Arduino String
    String str;
    int state = radio.readData(str);
    if (state == RADIOLIB_ERR_NONE) {
       // Parse and format output
      Serial.println("\n=== Sensor Data ===");
      Serial.println("Raw Data: " + str);
      
      // Parse key-value pairs
      parseSensorData(str);
      
      // Signal quality
      Serial.print("RSSI: ");
      Serial.print(radio.getRSSI());
      Serial.println(" dBm");
      Serial.print("SNR: ");
      Serial.print(radio.getSNR());
      Serial.println(" dB");
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("CRC error!"));

    } else {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(state);
    }
  }
}
