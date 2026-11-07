#pragma once
// The ES8311 codec library (esp_codec_dev) is T-Deck Max hardware; the Plus audio path (board/plus_audio.h) uses I2S
// directly, so the emulator needs only the include to resolve.
#include <Wire.h>
enum CodecType {CODEC_TYPE_ES8311};// same first enumerator as esp_codec.h (plus::Audio::begin takes it)
