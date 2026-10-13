#include <Arduino.h>
#include <Zigbee.h>

#include "Configuration.h"
#include "DoorDisplay.h"
#include "ZigbeeIlluminanceClient.h"

namespace {

    door_display::DoorDisplay display(door_display::kLedPrimaryPin, door_display::kLedSecondaryPin,
                                      door_display::kLedCount);
    door_display::ZigbeeIlluminanceClient illuminanceClient(door_display::kDisplayEndpoint,
                                                            display);

} // namespace

void setup() {
    Serial.begin(115200);
    display.begin();

    illuminanceClient.setManufacturerAndModel("ZigbeeDisplay", "DoorLuxV1");
    illuminanceClient.setPowerSource(ZB_POWER_SOURCE_MAINS);
    // Binding is initiated by resolveAndBind(), not by the library's
    // device-announcement based automatic binding path.
    illuminanceClient.setManualBinding(true);
    Zigbee.addEndpoint(&illuminanceClient);
    Zigbee.setRxOnWhenIdle(true);
    Zigbee.setPrimaryChannelMask(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

    Serial.println("Starting Zigbee end device");
    if (!Zigbee.begin(ZIGBEE_END_DEVICE)) {
        Serial.println("Zigbee failed to start; rebooting");
        delay(1000);
        ESP.restart();
    }

}

void loop() {
    static uint32_t nextBindingSearchMs = 0;
    static uint32_t nextLuxReadMs = 0;
    const bool zigbeeConnected = Zigbee.connected();

    // A battery sensor is normally asleep. Repeating this request gives it a
    // check-in opportunity to receive the directed Bind request.
    if (zigbeeConnected && !illuminanceClient.bindingConfigured() &&
        static_cast<int32_t>(millis() - nextBindingSearchMs) >= 0) {
        illuminanceClient.resolveAndBind();
        nextBindingSearchMs = millis() + door_display::kBindingSearchIntervalMs;
    }

    if (zigbeeConnected && illuminanceClient.sensorAddressKnown() &&
        illuminanceClient.bindingConfigured() && !display.hasIlluminance() &&
        static_cast<int32_t>(millis() - nextLuxReadMs) >= 0) {
        illuminanceClient.requestCurrentLux();
        nextLuxReadMs = millis() + door_display::kLuxReadIntervalMs;
    }

    display.update(zigbeeConnected);
    delay(door_display::kLoopDelayMs);
}
