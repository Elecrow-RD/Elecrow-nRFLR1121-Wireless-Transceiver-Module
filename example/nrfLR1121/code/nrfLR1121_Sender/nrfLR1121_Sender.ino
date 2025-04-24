#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <DHT20.h>
#include <Adafruit_TinyUSB.h>  // for Serial
#include <RadioLib.h>
#include <IRremote.hpp>

// Display Setup
U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, /* clock=*/7, /* data=*/6, /* reset=*/U8X8_PIN_NONE);  // All Boards without Reset of the Display
DHT20 dht20;

// Sensor variables
float temperature = 0;
float humidity = 0;

// LSM6DS3TR register addresses
#define LSM6DS3TR_ADDR 0x6B  // Default I2C address for LSM6DS3TR
#define CTRL1_XL 0x10        // Accelerometer control register
#define CTRL2_G 0x11         // Gyroscope control register
#define OUTX_L_XL 0x28       // Accelerometer X-axis data (low byte)

#define ACCEL_SENSITIVITY 0.000122  // Accelerometer sensitivity factor for 4g range (g -> m/s^2)

// Accelerometer functions
void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(LSM6DS3TR_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void readRegister(uint8_t reg, uint8_t *data, uint8_t length) {
  Wire.beginTransmission(LSM6DS3TR_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(LSM6DS3TR_ADDR, length);
  for (int i = 0; i < length; i++) {
    data[i] = Wire.read();
  }
}

void Axis_test() {
  // Configure accelerometer: 104Hz, 4g range
  writeRegister(CTRL1_XL, 0x40);

  uint8_t data[6];
  float accel[3];

  // Read accelerometer data
  readRegister(OUTX_L_XL, data, 6);
  for (int i = 0; i < 3; i++) {
    accel[i] = (int16_t)(data[i * 2] | (data[i * 2 + 1] << 8)) * ACCEL_SENSITIVITY * 9.80;
  }

  // Output accelerometer data to Serial Monitor
  Serial.print("\t\tAccel X: ");
  Serial.print(accel[0]);
  Serial.print(" \tY: ");
  Serial.print(accel[1]);
  Serial.print(" \tZ: ");
  Serial.print(accel[2]);
  Serial.println(" m/s^2 ");
}

// Constants for button and LED control
const int buttonPin = 12;  // the number of the pushbutton pin
const int ledPin = 26;     // the number of the LED pin

volatile bool ledState = false;  // LED state flag to handle button press

// LR1110 Radio Setup
LR1110 radio = new Module(44, 40, 42, 43);

// New: Define the sensor data structure
struct SensorData {
  float temperature;
  float humidity;
  float accelX;
  float accelY;
  float accelZ;
};

static const uint32_t rfswitch_dio_pins[] = {
  RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
  RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
  { LR11x0::MODE_STBY, { LOW, LOW } },
  { LR11x0::MODE_RX, { HIGH, LOW } },
  { LR11x0::MODE_TX, { HIGH, HIGH } },
  { LR11x0::MODE_TX_HP, { LOW, HIGH } },
  { LR11x0::MODE_TX_HF, { LOW, LOW } },
  { LR11x0::MODE_GNSS, { LOW, LOW } },
  { LR11x0::MODE_WIFI, { LOW, LOW } },
  END_OF_MODE_TABLE,
};

// Transmission state
int transmissionState = RADIOLIB_ERR_NONE;
volatile bool transmittedFlag = false;
int count = 0;

void setFlag(void) {
  transmittedFlag = true;
}

// IR Receiver setup
const uint8_t IR_RECEIVER_PIN = 24;  // Recommend GPIO4 (supporting PWM pins)
const uint8_t FEEDBACK_LED_PIN = 2;  // ESP32 build-in LED pin

// Button and LED control setup (for debounce and state toggle)
int Button = 32;                     // The pins for button connection
int LED = 41;                        // LED The connected pin
int state = 0;                       // LED state 0: OFF, 1: ON
unsigned long lastDebounceTime = 0;  // The time when the button was pressed last time
unsigned long debounceDelay = 50;    // De-jitter delay (50 milliseconds)
unsigned long lastButtonPressTime = 0;
unsigned long debounceDelay2 = 200;  // 200ms debounce delay
// Interrupt service function
void buttonISR() {
  unsigned long currentMillis = millis();
  // If the time difference between the current time and the last button trigger is greater than the de-jitter delay, the operation will be executed
  if (currentMillis - lastDebounceTime > debounceDelay) {
    state = 1 - state;  // toggle state
    if (state == 1) {
      digitalWrite(LED, HIGH);  // lighten LED
    } else {
      digitalWrite(LED, LOW);  // extinguish LED
    }
    lastDebounceTime = currentMillis;  // Update the de-jitter time
  }
}

void setup() {
  // Initialize the display
  u8g2.begin();
  Serial1.setPins(28, 29);
  // Initialize serial communication
  Serial.begin(9600);
  Serial1.begin(9600);  // Initialize Serial1 for communication

  // Initialize IR receiver
  IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK, FEEDBACK_LED_PIN);

  // Initialize DHT20 sensor
  Wire.setPins(13, 14);
  Wire.begin();
  while (dht20.begin()) {
    Serial.println("Initialize DHT20 sensor failed");
    delay(2000);
  }

  // Initialize button and LED pins
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // Attach interrupt for button press (RISING means button pressed)
  attachInterrupt(digitalPinToInterrupt(buttonPin), toggleLED, RISING);

  // Initialize LR1110 Radio
  SPI.setPins(47, 45, 46);
  SPI.begin();
  int state = radio.begin(915.0, 125.0, 10, 7, 0x34, 22, 8, 3.3);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Radio Initialized!"));
  } else {
    Serial.print(F("Radio Initialization Failed, code "));
    Serial.println(state);
    while (true) { delay(2000); }
  }
  radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);
  radio.setPacketSentAction(setFlag);

  // Start transmission of the first packet
  transmissionState = radio.startTransmit("Hello World!");


  // Setup button interrupt for LED control
  pinMode(Button, INPUT);
  pinMode(LED, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(Button), buttonISR, FALLING);
}

void loop() {
  SensorData data;
  data.temperature = dht20.getTemperature();
  data.humidity = dht20.getHumidity() * 100; // Convert to percentage

  // Display sensor readings on the screen
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.setCursor(0, 30);
    u8g2.print("Temp: ");
    u8g2.print(data.temperature);
    u8g2.print("C");
    u8g2.setCursor(0, 50);
    u8g2.print("Humidity: ");
    u8g2.print(data.humidity * 100);
    u8g2.print(" %RH");
  } while (u8g2.nextPage());

  // Output temperature and humidity to Serial monitor
  Serial.print("Temperature: ");
  Serial.print(data.temperature);
  Serial.print("C");
  Serial.print("  Humidity: ");
  Serial.print(data.humidity * 100);
  Serial.println(" %RH");

  // Read accelerometer data
  Axis_test();
  // Read accelerometer data
  uint8_t accelData[6];
  writeRegister(CTRL1_XL, 0x40);
  readRegister(OUTX_L_XL, accelData, 6);
  data.accelX = (int16_t)(accelData[0] | (accelData[1] << 8)) * ACCEL_SENSITIVITY * 9.80;
  data.accelY = (int16_t)(accelData[2] | (accelData[3] << 8)) * ACCEL_SENSITIVITY * 9.80;
  data.accelZ = (int16_t)(accelData[4] | (accelData[5] << 8)) * ACCEL_SENSITIVITY * 9.80;

  // 2. Build the sending string
  String payload =
    "Temp:" + String(data.temperature, 1) + ",Hum:" + String(data.humidity, 1) + ",X:" + String(data.accelX, 2) + ",Y:" + String(data.accelY, 2) + ",Z:" + String(data.accelZ, 2);
  // Handle Serial1 communication
  if (Serial1.available()) {
    while (Serial1.available()) {
      char incomingByte = Serial1.read();
      Serial.print(incomingByte);
    }
  }

  if (Serial.available()) {
    Serial1.write(Serial.read());
  }
  // IR signal handling
  if (IrReceiver.decode()) {
    handleIRCommand();
    IrReceiver.resume();
  }
  // Check if the previous transmission finished
  if (transmittedFlag) {
    transmittedFlag = false;
    if (transmissionState == RADIOLIB_ERR_NONE) {
      Serial.println(F("Transmission finished!"));
    } else {
      Serial.print(F("Transmission failed, code "));
      Serial.println(transmissionState);
    }
    radio.finishTransmit();
    transmissionState = radio.startTransmit(payload);
    Serial.println("Sent: " + payload);  // Debug output
  }



  delay(1000);  // Wait for the next loop
}
void toggleLED() {
  unsigned long currentMillis2 = millis();
  if (currentMillis2 - lastButtonPressTime > debounceDelay2) {
    ledState = !ledState;
    digitalWrite(ledPin, ledState ? HIGH : LOW);  // Set the LED state
    lastButtonPressTime = currentMillis2;         // Update the last press time
  }
}


// IR command handling function
void handleIRCommand() {
  digitalWrite(FEEDBACK_LED_PIN, HIGH);  // LED Flickering feedback

  Serial.print("- press -\t");

  // according to IrReceiver.decodedIRData.command Value judgment key
  switch (IrReceiver.decodedIRData.command) {
    case 0x45:  
      Serial.println("[CH-]");
      break;
    case 0x46:  
      Serial.println("[CH]");
      break;
    case 0x47: 
      Serial.println("[CH+]");
      break;
    case 0x44: 
      Serial.println("[PREV]");
      break;
    case 0x40:  
      Serial.println("[NEXT]");
      break;
    case 0x43: 
      Serial.println("[PLAY/PAUSE]");
      break;
    case 0x07:
      Serial.println("[VOL-]");
      break;
    case 0x15: 
      Serial.println("[VOL+]");
      break;
    case 0x09: 
      Serial.println("[EQ]");
      break;
    case 0x16: 
      Serial.println("[0]");
      break;
    case 0x19: 
      Serial.println("[100+]");
      break;
    case 0xD: 
      Serial.println("[200+]");
      break;
    case 0xC:
      Serial.println("[1]");
      break;
    case 0x18: 
      Serial.println("[2]");
      break;
    case 0x5E: 
      Serial.println("[3]");
      break;
    case 0x8: 
      Serial.println("[4]");
      break;
    case 0x1C: 
      Serial.println("[5]");
      break;
    case 0x5A: 
      Serial.println("[6]");
      break;
    case 0x42: 
      Serial.println("[7]");
      break;
    case 0x52: 
      Serial.println("[8]");
      break;
    case 0x4A: 
      Serial.println("[9]");
      break;
    default:
      Serial.println("[UNKNOWN]");
      break;
  }
  IrReceiver.resume();  // Continue to receive the next signal
}
