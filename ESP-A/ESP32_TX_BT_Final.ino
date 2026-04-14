#define USB_CDC_ON_BOOT 1

#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// ---- DS18B20 ----
#define DS18B20_PIN 4
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// ---- OLED (72x40) ----
// FIX #1: rotate so text is correct with USB-C at bottom
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);

// ---- BLE ----
const char* SERVICE_UUID   = "12345678-1234-1234-1234-1234567890ab";
const char* CHAR_TEMP_UUID = "abcd1234-5678-90ab-cdef-1234567890ab";

BLECharacteristic* tempChar;
bool deviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("BLE central connected");
  }
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("BLE central disconnected");
    // Restart advertising so Module B can reconnect
    BLEDevice::startAdvertising();
  }
};

void setupBle() {
  BLEDevice::init("KaitekiSensor");

  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  tempChar = pService->createCharacteristic(
    CHAR_TEMP_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  tempChar->addDescriptor(new BLE2902());  // enable notify

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE advertising as KaitekiSensor");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Module A: KaitekiSensor starting");

  sensors.begin();

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "KaitekiSensor");
  u8g2.sendBuffer();

  setupBle();
}

void loop() {
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);

  if (t < -50 || t > 125) {
    Serial.println("ERROR: DS18B20 read failed");

    // Optional: still show error on OLED (keeps behavior reasonable)
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(0, 14, "DS18B20 ERR");
    u8g2.sendBuffer();
  } else {
    // Serial debug
    Serial.print("Temp: ");
    Serial.print(t);
    Serial.println(" C");

    // Update OLED
    u8g2.clearBuffer();

// Update OLED
u8g2.clearBuffer();

if (!deviceConnected) {
  // ---- BT LOST: two-line display ----
  // Line 1: BT status (small font)
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 12, "BT LOST");

  // Line 2: Temperature (big font)
  u8g2.setFont(u8g2_font_logisoso24_tn);
  char disp[12];
  sprintf(disp, "%.1fC", t);
  u8g2.drawStr(0, 40, disp);   // bottom-aligned
} else {
  // ---- BT connected: big temperature only ----
  u8g2.setFont(u8g2_font_logisoso24_tn);
  char disp[12];
  sprintf(disp, "%.1fC", t);
  u8g2.drawStr(0, 40, disp);
}

u8g2.sendBuffer();

    u8g2.sendBuffer();

    // Send over BLE if a central is connected
    if (deviceConnected && tempChar) {
      char buf[16];
      sprintf(buf, "%.2f", t);  // string like "21.73"
      tempChar->setValue((uint8_t*)buf, strlen(buf));
      tempChar->notify();
      Serial.print("Notified BLE: ");
      Serial.println(buf);
    }
  }

  delay(1000);
}
