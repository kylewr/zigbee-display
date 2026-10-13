#pragma once

// The one-second startup rainbow is intentionally disabled. Its implementation
// is retained below so it can be restored without reconstructing it.
/*
void DoorDisplay::startStartupAnimation() {
    startupAnimationActive_ = true;
    xTaskCreate(startupAnimationTask, "led_startup", 2048, this, 6, nullptr);
}

void DoorDisplay::startupAnimationTask(void *context) {
    auto *display = static_cast<DoorDisplay *>(context);
    const uint32_t startedMs = millis();
    while (millis() - startedMs < kStartupAnimationDurationMs) {
        display->showStartupAnimation(millis() - startedMs);
        vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
    }
    display->normalStateNeedsRender_ = true;
    display->startupAnimationActive_ = false;
    vTaskDelete(nullptr);
}

void DoorDisplay::showStartupAnimation(uint32_t elapsedMs) {
    const uint16_t ledCount = primaryPixels_.numPixels();
    const uint16_t hueOffset =
        static_cast<uint16_t>(elapsedMs * 65536UL / kStartupAnimationDurationMs);
    for (uint16_t pixel = 0; pixel < ledCount; ++pixel) {
        const uint16_t hue =
            static_cast<uint16_t>(hueOffset + (65536UL * pixel / ledCount));
        const uint32_t color = primaryPixels_.gamma32(primaryPixels_.ColorHSV(hue));
        primaryPixels_.setPixelColor(pixel, color);
        secondaryPixels_.setPixelColor(pixel, color);
    }
    primaryPixels_.show();
    secondaryPixels_.show();
}
*/
