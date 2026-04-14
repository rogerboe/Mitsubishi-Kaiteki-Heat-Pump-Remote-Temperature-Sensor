#define USB_CDC_ON_BOOT 1

#include <Arduino.h>
#include <U8g2lib.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <HeatPump.h>

// ==================== OLED (ESP32-C3 SuperMini onboard 72x40) ====================
// Use U8G2_R0 or U8G2_R2 depending on your enclosure orientation.
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5); // change R0<->R2 if needed

// ==================== HeatPump / CN105 ====================
#define HP_RX 20   // ESP RX  <- CN105 pin 4 (TX from heat pump)
#define HP_TX 21   // ESP TX  -> CN105 pin 5 (RX to heat pump)
HeatPump hp;

// ==================== BLE (ESP-B as client) ====================
// Must match ESP-A (your code)
static BLEUUID SERVICE_UUID("12345678-1234-1234-1234-1234567890ab");
static BLEUUID CHAR_UUID   ("abcd1234-5678-90ab-cdef-1234567890ab");
static const char* TARGET_NAME = "KaitekiSensor";

static BLEAdvertisedDevice* foundDevice = nullptr;
static BLEClient* pClient = nullptr;
static BLERemoteCharacteristic* pRemoteChar = nullptr;

static bool btConnected = false;
static float lastTempC = NAN;
static uint32_t lastTempMs = 0;

// Inject rules: stop injecting when BT disconnected
static const uint32_t INJECT_INTERVAL_MS = 30000;  // inject every 30s while connected
static const uint32_t BLE_STALE_MS       = 120000; // stale after 2 minutes
static const float    MIN_CHANGE_C       = 0.2f;
static float lastInjectedC = NAN;

// Heat pump power tracking (for immediate inject on OFF->ON)
static bool lastHpPowerOn = false;

// ==================== Helpers ====================
static bool parseTemp(const std::string& s, float &outC) {
  if (s.empty()) return false;
  String a = String(s.c_str());
  a.trim();
  a.replace("C", "");
  a.trim();

  char* endptr = nullptr;
  float v = strtof(a.c_str(), &endptr);
  if (endptr == a.c_str()) return false;
  if (v < -50.0f || v > 125.0f) return false;
  outC = v;
  return true;
}

static void drawOLED() {
  u8g2.clearBuffer();

  if (!btConnected) {
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(0, 12, "BT LOST");

    if (!isnan(lastTempC)) {
      u8g2.setFont(u8g2_font_logisoso24_tn);
      char buf[12];
      snprintf(buf, sizeof(buf), "%.1fC", lastTempC);
      u8g2.drawStr(0, 40, buf);
    } else {
      u8g2.setFont(u8g2_font_6x12_tf);
      u8g2.drawStr(0, 30, "No temp");
    }
  } else {
    u8g2.setFont(u8g2_font_logisoso24_tn);
    char buf[12];
    if (!isnan(lastTempC)) snprintf(buf, sizeof(buf), "%.1fC", lastTempC);
    else snprintf(buf, sizeof(buf), "--.-C");
    u8g2.drawStr(0, 40, buf);
  }

  u8g2.sendBuffer();
}

// ==================== BLE callbacks ====================
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* /*client*/) override {
    btConnected = true;
    Serial.println("[BLE] Connected");
  }
  void onDisconnect(BLEClient* /*client*/) override {
    btConnected = false;
    pRemoteChar = nullptr;
    Serial.println("[BLE] Disconnected");
  }
};

static MyClientCallback clientCb;

static void notifyCB(BLERemoteCharacteristic* /*c*/, uint8_t* data, size_t len, bool /*isNotify*/) {
  std::string s((char*)data, (char*)data + len);
  float t;
  if (parseTemp(s, t)) {
    lastTempC = t;
    lastTempMs = millis();
    Serial.print("[BLE] Temp = ");
    Serial.println(lastTempC, 2);
  } else {
    Serial.print("[BLE] Notify (unparsed): ");
    Serial.println(s.c_str());
  }
}

// Scan for KaitekiSensor by Service UUID OR Name (robust)
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    bool matchSvc  = d.haveServiceUUID() && d.isAdvertisingService(SERVICE_UUID);
    bool matchName = d.haveName() && (d.getName() == TARGET_NAME);

    if (matchSvc || matchName) {
      Serial.print("[BLE] Found target: ");
      Serial.println(d.toString().c_str());

      if (foundDevice) delete foundDevice;
      foundDevice = new BLEAdvertisedDevice(d);

      BLEDevice::getScan()->stop();
    }
  }
};

static void startScan() {
  Serial.println("[BLE] Scanning...");
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  scan->setActiveScan(true);
  scan->start(5, false);
}

static bool connectToServer() {
  if (!foundDevice) return false;

  Serial.println("[BLE] Connecting...");
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(&clientCb);

  if (!pClient->connect(foundDevice)) {
    Serial.println("[BLE] Connect failed");
    return false;
  }

  BLERemoteService* svc = pClient->getService(SERVICE_UUID);
  if (!svc) {
    Serial.println("[BLE] Service not found");
    pClient->disconnect();
    return false;
  }

  pRemoteChar = svc->getCharacteristic(CHAR_UUID);
  if (!pRemoteChar) {
    Serial.println("[BLE] Characteristic not found");
    pClient->disconnect();
    return false;
  }

  if (pRemoteChar->canNotify()) {
    pRemoteChar->registerForNotify(notifyCB);
    Serial.println("[BLE] Notifications enabled");
  } else {
    Serial.println("[BLE] Characteristic cannot notify");
  }

  btConnected = true;
  return true;
}

// ==================== Remote temp injection logic ====================
static bool haveFreshBleTemp() {
  return !isnan(lastTempC) && (millis() - lastTempMs) < BLE_STALE_MS;
}

static bool shouldInjectNow(bool force) {
  if (!haveFreshBleTemp()) return false;
  if (force) return true;
  return isnan(lastInjectedC) || fabsf(lastTempC - lastInjectedC) >= MIN_CHANGE_C;
}

static void injectRemoteTempNow() {
  Serial.print("[HP] Inject remote temp = ");
  Serial.println(lastTempC, 2);

  hp.setRemoteTemperature(lastTempC);
  hp.update();
  lastInjectedC = lastTempC;
}

// ==================== Setup / Loop ====================
void setup() {
  Serial.begin(115200);
  delay(300);

  // OLED boot splash
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 12, "ESP-B BOOT");
  u8g2.sendBuffer();

  // HeatPump CN105
  Serial.println("[HP] Connecting CN105...");
  hp.connect(&Serial1, 2400, HP_RX, HP_TX);
  hp.setFastSync(true);

  // Allow MELCloud / IR changes to stick
  hp.enableExternalUpdate();

  // Initialize last power state (may be 0/false until first packets arrive)
  lastHpPowerOn = hp.getPowerSettingBool();

  // BLE init (client)
  BLEDevice::init("KaitekiGateway");
  startScan();
}

void loop() {
  // Keep CN105 protocol alive
  hp.sync();

  // BLE connect/reconnect logic
  static uint32_t lastBleAttempt = 0;

  if (!btConnected) {
    if (foundDevice && (millis() - lastBleAttempt > 2000)) {
      lastBleAttempt = millis();
      (void)connectToServer();
    } else if (!foundDevice && (millis() - lastBleAttempt > 5000)) {
      lastBleAttempt = millis();
      startScan();
    }
  } else {
    if (pClient && !pClient->isConnected()) {
      btConnected = false;
      pRemoteChar = nullptr;
      Serial.println("[BLE] Lost connection");
    }
  }

  // OLED refresh
  static uint32_t lastOled = 0;
  if (millis() - lastOled > 500) {
    lastOled = millis();
    drawOLED();
  }

  // ==================== Inject ONLY when:
  // 1) BLE connected
  // 2) Heat pump is ON
  // plus: inject immediately when OFF->ON (force)
  bool hpPowerOn = hp.getPowerSettingBool();
  bool turnedOnNow = (!lastHpPowerOn && hpPowerOn);
  lastHpPowerOn = hpPowerOn;

  // Immediate inject when the heat pump turns ON (if BLE temp available)
  if (btConnected && hpPowerOn && turnedOnNow) {
    if (shouldInjectNow(true)) {
      Serial.println("[HP] Power turned ON -> immediate inject");
      injectRemoteTempNow();
    } else {
      Serial.println("[HP] Power ON but no fresh BLE temp yet");
    }
  }

  // Periodic inject while ON + BLE connected
  static uint32_t lastInject = 0;
  if (btConnected && hpPowerOn && (millis() - lastInject > INJECT_INTERVAL_MS)) {
    lastInject = millis();

    if (!haveFreshBleTemp()) {
      Serial.println("[HP] No recent BLE temp, skip inject");
    } else if (shouldInjectNow(false)) {
      injectRemoteTempNow();
    } else {
      Serial.println("[HP] Temp change < 0.2C, skip inject");
    }
  }

  // If BLE disconnects OR heat pump is OFF: we inject nothing (your requirement)
}