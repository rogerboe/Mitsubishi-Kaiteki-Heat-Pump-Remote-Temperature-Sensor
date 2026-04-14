🌡️ Mitsubishi Kaiteki External Temperature Sensor (CN105 + ESP32-C3)

📖 Overview

This project replaces the internal temperature sensing behavior of a Mitsubishi Kaiteki (MSZ-LN series) heat pump by injecting an external room temperature via the CN105 service port.

It uses two ESP32-C3 boards:
	•	ESP-A (Sensor Node)
Reads temperature from a DS18B20 and broadcasts via BLE
	•	ESP-B (Gateway Node)
Receives BLE temperature and injects it into the heat pump via CN105 UART

✅ Works without MELCloud
✅ Coexists with MELCloud and IR remote
✅ Automatic fallback to internal sensor

⸻

🧠 System Architecture

[ DS18B20 Sensor ]
        ↓
   ESP32-C3 (ESP-A)
   - BLE Server
   - OLED Display
        ↓ (BLE Notify)
   ESP32-C3 (ESP-B)
   - BLE Client
   - CN105 UART
        ↓
[ Mitsubishi Heat Pump CN105 ]

🔧 Hardware Components

ESP32 Boards
	•	Model: ESP32-C3 SuperMini
	•	Built-in SSD1306 OLED (72×40)
	•	USB-C power
	•	BLE 5 support

⸻

Temperature Sensor
	•	Model: DS18B20 (digital)
	•	Library: OneWire + DallasTemperature

⸻

🔌 Wiring

📍 DS18B20 → ESP-A
DS18B20              ESP-A
VCC                  3.3V
GND                  GND
DATA                 GPIO4

Pull-up resistor (required)
3.3V ---[4.7kΩ]--- DATA (GPIO4)

🔌 CN105 Connection (ESP-B)

CN105 Pinout

Pin            Signal
1              12V
2              GND
3              5V
4              TX (from heat pump)
5              RX (to heat pump)

UART Wiring

CN105            ESP32-C3
Pin 2 (GND)      GND
Pin 4 (TX)       GPIO20 (RX)
Pin 5 (RX)       GPIO21 (TX)

⚡ Important Electrical Details

Open-Collector TX (CRITICAL)

CN105 TX is open-collector, so it requires a pull-up:

CN105 TX ----+---- GPIO20
             |
           [10kΩ]
             |
            3.3V

	•	Value: 4.7kΩ – 10kΩ


TX Protection (ESP → Heat Pump)
GPIO21 ----[1kΩ]---- CN105 Pin 5

	•	Protects ESP from 5V pull-up on CN105 RX

Powering
	•	Development: USB
	•	Optional: CN105 Pin 3 (5V) with protection (diode recommended)

⸻

📡 Communication

UART Settings
	•	2400 baud
	•	8E1 (Even parity)

Protocol
	•	Mitsubishi CN105 (reverse engineered)
	•	Uses SwiCago HeatPump library

⸻

🧠 Software Behavior

ESP-A (Sensor Node)
	•	Reads DS18B20
	•	Sends temperature via BLE notify
	•	OLED:
	•	Temperature when connected
	•	BT LOST when disconnected

⸻

ESP-B (Gateway Node)
	•	Connects to ESP-A via BLE
	•	Receives temperature
	•	Injects temperature via:

hp.setRemoteTemperature(temp);
hp.update();


⸻

⚙️ Smart Injection Logic

Temperature is injected only when:
	•	✅ BLE connected
	•	✅ Heat pump is ON
	•	✅ Data is recent

Features
	•	Stops injecting if BLE disconnects
	•	Immediate injection when heat pump turns ON
	•	Ignores small changes (<0.2°C)
	•	Automatic fallback to internal sensor

⸻

☁️ MELCloud Compatibility

MELCloud remains fully functional:
	•	Power ON/OFF
	•	Mode changes (HEAT/COOL/etc.)
	•	Set temperature

Required setting:
hp.enableExternalUpdate();

Without this, ESP will override MELCloud commands.

🧪 Verified Behavior

✔ External sensor controls heating
✔ Unit reacts to temperature changes (window test confirmed)
✔ Automatic fallback when ESP-A disconnects
✔ No lock-in or unsafe state

⸻

⚠️ Notes
	•	MELCloud temperature may differ slightly (normal)
	•	Internal averaging
	•	Sensor placement
	•	Reporting delay

⸻

🚀 Future Improvements
	•	Temperature calibration offset
	•	Averaging / smoothing
	•	MQTT integration
	•	Web UI
	•	Fail-safe fallback temperature

⸻

🙏 Credits
	•	SwiCago HeatPump Library
	•	Mitsubishi CN105 reverse engineering community
	•	Nicegear blog (archived)

⸻

📜 License
	•	HeatPump library: LGPL
	•	This project: same terms unless otherwise specified

⸻

💡 Project Status

✅ Fully working
🔧 Production-ready setup
🚀 Ready for expansion



