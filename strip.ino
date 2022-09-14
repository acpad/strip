// © Kay Sievers <kay@versioduo.com>, 2022
// SPDX-License-Identifier: Apache-2.0

#include <V2Base.h>
#include <V2Buttons.h>
#include <V2Device.h>
#include <V2LED.h>
#include <V2MIDI.h>

V2DEVICE_METADATA("com.acpad.strip", 2, "versioduo:samd:itsybitsy");

static constexpr uint16_t n_leds = 41;
static V2LED::WS2812 LED(n_leds, 5, &sercom2, SPI_PAD_3_SCK_1, PIO_SERCOM);

static class Device : public V2Device {
public:
  Device() : V2Device() {
    metadata.vendor      = "ACPAD Instruments GmbH";
    metadata.product     = "ACPAD strip";
    metadata.description = "LED Strip";
    metadata.home        = "https://acpad.com/#strip";

    system.download  = "https://support.acpad.com/download";
    system.configure = "https://support.acpad.com/configure";

    usb.ports.standard = 0;
  }

private:
  void handleControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {
    switch (controller) {

      case V2MIDI::CC::AllSoundOff:
      case V2MIDI::CC::AllNotesOff:
        LED.reset();
        break;
    }
  }

  void handleSystemExclusive(const uint8_t *buffer, uint32_t len) override {
    if (len < 10)
      return;

    // 0x7d == SysEx prototype/research/private ID
    if (buffer[1] != 0x7d)
      return;

    // Handle only JSON messages.
    if (buffer[2] != '{' || buffer[len - 2] != '}')
      return;

    // Read incoming message.
    StaticJsonDocument<4096> json;
    if (deserializeJson(json, buffer + 2, len - 1))
      return;

    JsonObject json_led = json["led"];
    if (!json_led)
      return;

    JsonArray json_colors = json_led["colors"];
    if (!json_colors)
      return;

    const uint8_t start = json_led["start"];
    for (uint8_t i = 0; i + start < n_leds; i++) {
      JsonArray json_color = json_colors[i];
      if (!json_color)
        break;

      // Empty array, skip LED.
      if (json_color[2].isNull())
        continue;

      uint8_t hue = json_color[0];
      if (hue > 127)
        hue = 127;

      uint8_t saturation = json_color[1];
      if (saturation > 127)
        saturation = 127;

      uint8_t brightness = json_color[2];
      if (brightness > 127)
        brightness = 127;

      LED.setHSV(i + start, (float)hue / 127.f * 360.f, (float)saturation / 127.f, (float)brightness / 127.f);
    }
  }
} Device;

// Dispatch MIDI packets
static class MIDI {
public:
  void loop() {
    if (!Device.usb.midi.receive(&_midi))
      return;

    if (_midi.getPort() != 0)
      return;

    Device.dispatch(&Device.usb.midi, &_midi);
  }

private:
  V2MIDI::Packet _midi{};
} MIDI;

void setup() {
  Serial.begin(9600);

  LED.begin();
  LED.setMaxBrightness(0.5);

  Device.begin();
  Device.reset();
}

void loop() {
  LED.loop();
  MIDI.loop();
  Device.loop();

  if (Device.idle())
    Device.sleep();
}
