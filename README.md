# Zigbee Door Display

An ESP32-C6 SuperMini drives five WS2812-compatible LEDs to show the light and
motion reported by a Zigbee sensor. It is a Zigbee end device with its receiver
on while idle; it does not use Wi-Fi, Home Assistant automations, or a cloud
service.

## Hardware

- ESP32-C6 SuperMini
- Five-pixel WS2812/NeoPixel chain on either GPIO0 or GPIO17
- Zigbee illuminance and occupancy sensor, currently configured for a SONOFF
  SNZB-03PR2
- Always-powered Zigbee coordinator

Power the LEDs from a suitable external supply and share ground with the C6.
Use a 3.3 V-compatible first pixel or a level shifter if the LED chain requires
a 5 V data signal.

The same firmware drives both GPIO0 and GPIO17. Connect the chain only to the
pin used by that display; the other output is harmlessly unused.

## How it works

```text
sensor --direct Zigbee reports--> display
   ^                                  |
   +-------- coordinator-assisted binding
```

On startup, the display asks the coordinator for the sensor's current short
address, then asks the sensor to bind its Illuminance Measurement (`0x0400`)
and Occupancy Sensing (`0x0406`) clusters to display endpoint 10. The sensor
then reports directly to every bound display.

The coordinator must remain powered. Add a mains-powered Zigbee router only
when radio range requires it; a router does not relay sensor reports at the
application level.

## Configure, build, and flash

Edit these values in [platformio.ini](platformio.ini):

| Setting | Purpose |
| --- | --- |
| `DOOR_SENSOR_IEEE` | Sensor's fixed 16-digit IEEE/EUI-64, with `ULL` appended |
| `DOOR_DARK_LUX` | Switch to dark at or below this value |
| `DOOR_LIGHT_LUX` | Switch to light at or above this value |
| `DOOR_STALE_SECONDS` | Time without a valid lux report before showing an error |

For example, use `0x00124B0012345678ULL` for `DOOR_SENSOR_IEEE`. Do not use the
sensor's changing short address.

```sh
pio run
pio run --target upload
pio device monitor
```

PIOArduino provides the Zigbee stack and its `zigbee.csv` partition table,
which preserves Zigbee network storage. Avoid erasing flash unless you intend
to reset pairing.

## LED states

| Display | Meaning |
| --- | --- |
| Sweeping amber dot | Joining or searching for the Zigbee network |
| Sweeping magenta dot | Joined, but no recent valid lux report |
| Dim blue | Dark |
| Solid red | Light |
| Solid yellow | Motion detected while dark or before a lux reading |

The status dot travels from the first pixel to the last in 600 ms and takes
another 600 ms to return. Lux is authoritative: motion does not override a
known light state. Readings between the configured dark and light thresholds
retain the previous light state; `0xffff` is ignored as invalid.

## First-time check

1. Set `DOOR_SENSOR_IEEE`, then open joining on the coordinator and flash the
   display.
2. Confirm the display joins: its endpoint 10 should expose Illuminance and
   Occupancy Sensing client clusters.
3. Wake the sensor if needed. The serial log should show the illuminance and
   motion bindings being accepted.
4. Check that lux reports change the LEDs, then tune the thresholds. Trigger
   motion to confirm the yellow state.

The sensor's direct-binding capacity and report behavior should be validated on
the final hardware before relying on multiple displays.

## References

- [SONOFF SNZB-03PR2](https://sonoff.tech/products/sonoff-senseguard-motion-gen2-zigbee-motion-sensor-snzb-03pr2)
- [Zigbee2MQTT SNZB-03PR2 notes](https://www.zigbee2mqtt.io/devices/SNZB-03PR2.html)
- [Espressif Zigbee SDK](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/)
