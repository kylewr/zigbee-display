#pragma once

#include <Adafruit_NeoPixel.h>

#include <cstdint>

namespace door_display {

    class DoorDisplay final {
        public:
            DoorDisplay(uint8_t primaryLedPin, uint8_t secondaryLedPin, uint8_t ledCount);

            void begin();
            void recordIlluminance(uint16_t rawValue);
            void recordOccupancy(bool detected);
            bool hasIlluminance() const;
            void update(bool zigbeeConnected);

        private:
            enum class State : uint8_t { Joining, Stale, Dark, Light, Motion };

            static uint16_t luxToZcl(unsigned lux);
            uint8_t statusSweepPixel(uint32_t now) const;
            void showStartupAnimation(uint32_t elapsedMs);
            void showState(State state, uint8_t statusPixel);

            Adafruit_NeoPixel primaryPixels_;
            Adafruit_NeoPixel secondaryPixels_;
            mutable portMUX_TYPE stateMux_ = portMUX_INITIALIZER_UNLOCKED;
            bool haveLux_ = false;
            bool isLight_ = false;
            bool motionDetected_ = false;
            uint32_t lastReportMs_ = 0;
            uint32_t startupAnimationStartedMs_ = 0;
            bool startupAnimationFinished_ = false;
            State previousState_ = State::Joining;
            uint8_t previousStatusPixel_ = 0;
    };

} // namespace door_display
