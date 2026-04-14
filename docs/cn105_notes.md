# CN105 Notes

## CN105 Pinout (Verified)

| Pin | Signal |
|-----|--------|
| 1   | 12V    |
| 2   | GND    |
| 3   | 5V     |
| 4   | TX (from heat pump) |
| 5   | RX (to heat pump)   |


## Electrical Characteristics

- CN105 TX behaves as **open-collector**
- Requires external pull-up (4.7k–10k to 3.3V)
- Safe to connect directly to ESP32-C3 RX (verified)
- No series resistor required on RX

## UART Configuration

- Baud: 2400
- Format: 8E1 (8 data bits, even parity, 1 stop bit)
- ESP32-C3 pins:
  - RX: GPIO20
  - TX: GPIO21

 ## Protocol Behavior

- Device is **not chatty**
- Requires **CONNECT (0x5A)** before responding
- Responds with:
  - 0x7A → Connect ACK
  - 0x62 → Data response

- GET request must use correct payload
- Working payload discovered:
  - First byte = 0x02 → returns valid status
 
## Example Frames

### Connect Request
FC 5A 01 30 02 CA 01 A8

### Connect Response
FC 7A 01 30 01 00 54

### Working GET Request
FC 42 01 30 10 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 7B

### Example Response
FC 62 01 30 10 02 00 00 01 09 09 06 00 00 00 80 AC 28 00 00 00 EE

## Remote Temperature Injection

- Injected using:
  hp.setRemoteTemperature(value)

- Behavior:
  - Overrides internal sensor temporarily
  - Must be refreshed periodically (~30–60s)
  - Automatically falls back to internal sensor when stopped

- No explicit “disable” command required

  ## MELCloud Interaction

- MELCloud can coexist with CN105 control
- Requires:
  hp.enableExternalUpdate()

- Without this:
  - ESP overrides MELCloud changes
  - Unit may turn itself back ON


## Known Issues / Observations

- Internal vs injected temperature may differ from MELCloud display
- Small delay before injected temperature appears in status
- BLE disconnect correctly stops injection
- System fails safely to internal sensor

  ## References

- SwiCago HeatPump library
- Nicegear CN105 reverse engineering blog
- Mitsubishi service documentation

