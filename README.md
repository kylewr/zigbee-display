# Zigbee door display prototype

This firmware runs on one ESP32-C6 SuperMini per display. It is a Zigbee **end
device** with its receiver on while idle. A single chain of five WS2812
compatible pixels is driven from GPIO0. It uses no Wi-Fi, Home Assistant
automation, or cloud API. Each display applies its own lux thresholds.
The `HA` profile name in the Zigbee SDK means Zigbee Home Automation; it does
not imply Home Assistant software.

## Network design

```text
ESP32-C6 running tostmann/esp-coordinator
 (Zigbee coordinator/NCP, continuously powered)
                    |
          optional Zigbee router(s)
                    |
       SONOFF SNZB-03PR2  --direct 0x0400 + 0x0406 bindings-->  display(s)
          (end device)                              (RX-on end devices)
```

The linked `tostmann/esp-coordinator` firmware is a Zigbee coordinator and
ZBOSS network co-processor for Zigbee2MQTT. It is not router firmware. A
separate router is unnecessary when the coordinator has adequate radio range
and direct-child capacity. Add a mains-powered Zigbee router only if a device
cannot maintain a reliable path to the coordinator; a router forwards packets
but does not copy sensor reports into the displays.

Each display asks the coordinator for the configured sensor's current short
address every 10 seconds, then sends that sensor ZDO Bind requests. The
sensor's IEEE/EUI-64 is fixed, but its short address can change after a rejoin;
the coordinator lookup handles that change. The bindings are stored in the
**sensor**, from its Illuminance Measurement (`0x0400`) and Occupancy Sensing
(`0x0406`) servers on endpoint 1 to that display's IEEE address on endpoint 10.
This avoids the unavailable ZHA binding UI. Every added display creates a
separate destination entry for each cluster, so one sensor can report directly
to multiple displays. Keep the coordinator hardware powered; the host is not
in the direct report path after binding.

The sensor must also emit MeasuredValue (`0x0000`) reports. Try one display
first and verify reports on its serial log before binding a second. Binding
table capacity and multi-destination behavior of this exact SONOFF firmware
are not yet verified. The display currently accepts any valid `0x0400/0x0000`
report addressed to endpoint 10; use one light sensor per Zigbee network until
a source-address filter is added.

If the SONOFF cannot bind its illuminance cluster to the displays, buying a
generic router will not fix the data path. A fallback is an always-on
application relay that receives the sensor report and sends a display command.
That relay can live in a custom coordinator or a dedicated mains-powered C6
application node, but merely assigning that node the Zigbee router role is
insufficient.

If the coordinator is USB-powered by the Home Assistant computer, shutting
the computer down may also remove power from the Zigbee coordinator. Give the
coordinator an always-on power path for the final installation. Pairing,
interviews, backups, and network management still require its host application.

## Build and flash with PIOArduino

This is an Arduino-framework project using the PIOArduino ESP32 platform. The
firmware source is `src/main.cpp`; the Arduino ESP32 Zigbee library supplies
the ZCL client/report callbacks.

```sh
pio run
pio run --target upload
pio device monitor
```

On the current development machine, the shell's pyenv `pio` shim is an older
PlatformIO Core 6.1.19 while the PIOArduino environment uses Core 6.2.0. Use
the PIOArduino VS Code actions, or run `~/.platformio/penv/bin/pio` explicitly,
to avoid loading the wrong SCons installation.

To change the illustrative 15/30 lux thresholds or the 30-minute stale timeout,
edit `DOOR_DARK_LUX`, `DOOR_LIGHT_LUX`, and `DOOR_STALE_SECONDS` in
`platformio.ini`.

Set `DOOR_SENSOR_IEEE` in `platformio.ini` to the sensor's 16 hexadecimal digit
IEEE/EUI-64, with `ULL` appended, for example
`0x00124B0012345678ULL`. This is the only per-sensor configuration needed by
every display. Do not use the changing Zigbee short address.

PIOArduino supplies the Zigbee stack and downloads Adafruit NeoPixel on the
first build. Its `zigbee.csv` partition table includes Zigbee network storage.
Keep this partition when reflashing. Do not erase flash unless intentionally
resetting pairing. The device joins an open Zigbee network on first boot and
should reuse stored network state after normal power cycles.

The example assumes a 5 V WS2812/NeoPixel chain with shared ground and an
appropriate GPIO0-to-DIN signal path. Use a 3.3 V compatible first pixel or a
data level shifter if the chain needs a 5 V logic high. Power the pixels from
a supply sized for the strip, not directly from a GPIO. Confirm the actual
SuperMini board's GPIO0 pinout before connecting it.

## LED states and tuning

The pixels show these states:

| Color | Meaning |
| --- | --- |
| Sweeping amber dot | Joining Zigbee |
| Sweeping magenta dot | Joined, but no recent valid lux report |
| Dim blue | Dark |
| Solid red (`255, 0, 0`) | Light |
| Solid yellow (`255, 255, 0`) | Motion detected |

The status dot takes 600 ms to travel from the first pixel to the last, then
600 ms to return. Defaults are illustrative: `<=15 lx` becomes dark, `>=30 lx` becomes light,
and readings in between retain the previous state. The first reading in the
middle band selects dark. A report of `0` means below the sensor's measuring
range; `0xffff` is ignored as invalid. If there is no valid report for 30
minutes, the display turns red. Adjust all three settings in `platformio.ini`
after logging real values from the final sensor position. Lux is authoritative:
when the last lux value is light, the display stays red even during motion. A
motion-detected report selects yellow only while the cached lux state is dark
or unavailable. A motion-clear report does not update, age, or otherwise
change the cached illuminance decision; it returns the display to its existing
red or blue lux state. This preserves the light state when somebody is
stationary behind a shower curtain.

After the sensor accepts the bindings, a display reboot asks for the current
MeasuredValue every 10 seconds until it receives a valid value. It avoids
competing read commands with the initial binding transaction. A sleeping sensor
can still defer that response until its next check-in.

Zigbee encodes illuminance as `1 + 10000 * log10(lux)`; the firmware compares
the encoded values and prints an approximate lux value for calibration.

## Bring-up checks

1. Confirm the actual sensor model and that it exposes `0x0400/0x0000` on
   its interviewed endpoint.
2. Pair a display while the coordinator allows joining. Confirm endpoint 10
   has Illuminance Measurement and Occupancy Sensing **client** clusters and
   the LEDs change from amber to red.
3. Set `DOOR_SENSOR_IEEE`, then flash each display. Briefly press the sensor
   button to wake it. The display log should show `Sensor is at ...`, followed
   by `Illuminance binding accepted by sensor`, then `Motion binding accepted
   by sensor; bindings ready`. Repeat this for each display.
4. Move between bright and dark conditions. Confirm `Lux report ...` appears
   in the display log, then tune thresholds. Move in front of the sensor and
   confirm `Motion detected ...` and the yellow display; after `Motion clear
   ...`, it should return to the prior lux color.
5. Pair and bind a second display. Confirm both receive reports over several
   hours and across power cycles.
6. Stop the coordinator host while keeping the coordinator powered. Confirm
   both displays continue receiving reports. If they stop, the design needs an
   always-on application relay; adding a plain router will not help.

The firmware has not yet been flashed to the target hardware. The binding
behavior and report interval need validation with the real sensor and
coordinator.

## References

- [SONOFF SNZB-03PR2 product details](https://sonoff.tech/products/sonoff-senseguard-motion-gen2-zigbee-motion-sensor-snzb-03pr2)
- [Zigbee2MQTT SNZB-03PR2 definition](https://www.zigbee2mqtt.io/devices/SNZB-03PR2.html)
- [Zigbee2MQTT converter source](https://github.com/Koenkk/zigbee-herdsman-converters/blob/master/src/devices/sonoff.ts)
- [tostmann/esp-coordinator](https://github.com/tostmann/esp-coordinator)
- [Zigbee2MQTT ZBOSS adapter notes](https://www.zigbee2mqtt.io/guide/adapters/zboss.html)
- [Espressif Zigbee SDK 1.x ZCL cluster API](https://docs.espressif.com/projects/esp-zigbee-sdk/en/release-v1.0/esp32c6/api-reference/esp_zigbee_cluster.html)
- [Espressif Zigbee SDK network persistence FAQ](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/faq.html)
