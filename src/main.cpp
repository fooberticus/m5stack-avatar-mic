// Copyright(c) 2023 Takao Akaki

#include <M5Unified.h>
#include <Avatar.h>
#include "fft.hpp"
#include <cinttypes>

#if defined(ARDUINO_M5STACK_CORES3)
#include <gob_unifiedButton.hpp>
gob::UnifiedButton unifiedButton;
#endif
#define USE_MIC

#ifdef USE_MIC
// ---------- Mic sampling ----------

#define READ_LEN    (2 * 256)
#define LIPSYNC_LEVEL_MAX 10.0f

static fft_t fft;
static constexpr size_t WAVE_SIZE = 256 * 2;

static constexpr const size_t record_samplerate = 16000;
static int16_t *rec_data;

// Could be rewritten to identify the model at the beginning of setup.
// Please test those as well. (Due to different microphone performance)
uint8_t lipsync_shift_level = 11; // Set how small the lip sync data should be. The way you open your mouth changes.
float lipsync_max = LIPSYNC_LEVEL_MAX;  // Lip sync unit If you increase or decrease this, the way your mouth opens will change.

#endif

static uint8_t display_rotation = 1; // Display orientation (0-3)

using namespace m5avatar;

Avatar avatar;
ColorPalette *cps[6];
uint8_t palette_index = 0;

uint32_t last_rotation_msec = 0;
uint32_t last_lipsync_max_msec = 0;

void lipsync() {
    uint64_t level = 0;
    if (M5.Mic.record(rec_data, WAVE_SIZE, record_samplerate)) {
        fft.exec(rec_data);
        for (size_t bx = 5; bx <= 60; ++bx) {
            int32_t f = fft.get(bx);
            level += abs(f);
        }
    }
    uint32_t temp_level = level >> lipsync_shift_level;
    M5_LOGI("level:%" PRId64 "\n", level);         // Comment out this line when adjusting lipsync_max.
    M5_LOGI("temp_level:%d\n", temp_level);        // Comment out this line when adjusting lipsync_max.
    auto ratio = (float) (temp_level / lipsync_max);
    M5_LOGI("ratio:%f\n", ratio);
    if (ratio <= 0.01f) {
        ratio = 0.0f;
        if ((millis() - last_lipsync_max_msec) > 500) {
            // If there is no sound for more than 0.5 seconds, reset the lip sync limit.
            last_lipsync_max_msec = millis();
            lipsync_max = LIPSYNC_LEVEL_MAX;
        }
    } else {
        if (ratio > 1.3f) {
            if (ratio > 1.5f) {
                // If the lip sync limit is significantly exceeded, the limit will be raised.
                lipsync_max += 10.0f;
            }
            ratio = 1.3f;
        }
        last_lipsync_max_msec = millis(); // Update if not silent
    }

    if ((millis() - last_rotation_msec) > 350) {
        int direction = random(-2, 2);
        avatar.setRotation(direction * 10 * ratio);
        last_rotation_msec = millis();
    }
    avatar.setMouthOpenRatio(ratio);
}


void setup() {
    auto cfg = M5.config();
    cfg.internal_mic = true;
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
#if defined( ARDUINO_M5STACK_CORES3 )
    unifiedButton.begin(&M5.Display, gob::UnifiedButton::appearance_t::transparent_all);
#endif
    M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
    M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
    M5.Log.setEnableColor(m5::log_target_serial, false);
    M5_LOGI("Avatar Start");
    M5.Log.printf("M5.Log avatar Start\n");
    Serial.println("avatar start");
    float scale = 0.0f;
    int8_t position_top = 0;
    int8_t position_left = 0;
    uint8_t first_cps = 0;
    auto mic_cfg = M5.Mic.config();

    switch (M5.getBoard()) {
        case m5::board_t::board_M5AtomS3:
            first_cps = 4;
            scale = 0.55f;
            position_top = -60;
            position_left = -95;
            display_rotation = 3;
            // M5AtomS3 is an external microphone (PDMUnit), so configure settings.
            mic_cfg.pin_ws = 1;
            mic_cfg.pin_data_in = 2;
            M5.Mic.config(mic_cfg);
            break;

        case m5::board_t::board_M5StickC:
            first_cps = 1;
            scale = 0.6f;
            position_top = -80;
            position_left = -80;
            display_rotation = 3;
            break;

        case m5::board_t::board_M5StickCPlus:
            first_cps = 1;
            scale = 0.85f;
            position_top = -55;
            position_left = -35;
            display_rotation = 3;
            break;

        case m5::board_t::board_M5StackCore2:
            scale = 1.0f;
            position_top = 0;
            position_left = 0;
            display_rotation = 1;
            break;

        case m5::board_t::board_M5StackCoreS3:
            scale = 1.0f;
            position_top = 0;
            position_left = 0;
            display_rotation = 1;
            break;

        case m5::board_t::board_M5Stack:
            scale = 1.0f;
            position_top = 0;
            position_left = 0;
            display_rotation = 1;
            break;


        default:
            Serial.println("Invalid board.");
            break;
    }
    rec_data = (typeof(rec_data)) heap_caps_malloc(WAVE_SIZE * sizeof(int16_t), MALLOC_CAP_8BIT);
    memset(rec_data, 0, WAVE_SIZE * sizeof(int16_t));
    M5.Speaker.end();
    M5.Mic.begin();

    M5.Display.setRotation(display_rotation);
    avatar.setScale(scale);
    avatar.setPosition(position_top, position_left);
    avatar.init(1); // start drawing
    cps[0] = new ColorPalette();
    cps[0]->set(COLOR_PRIMARY, TFT_BLACK);
    cps[0]->set(COLOR_BACKGROUND, TFT_YELLOW);
    cps[1] = new ColorPalette();
    cps[1]->set(COLOR_PRIMARY, TFT_BLACK);
    cps[1]->set(COLOR_BACKGROUND, TFT_ORANGE);
    cps[2] = new ColorPalette();
    cps[2]->set(COLOR_PRIMARY, (uint16_t) 0x00ff00);
    cps[2]->set(COLOR_BACKGROUND, (uint16_t) 0x303303);
    cps[3] = new ColorPalette();
    cps[3]->set(COLOR_PRIMARY, TFT_WHITE);
    cps[3]->set(COLOR_BACKGROUND, TFT_BLACK);
    cps[4] = new ColorPalette();
    cps[4]->set(COLOR_PRIMARY, TFT_BLACK);
    cps[4]->set(COLOR_BACKGROUND, TFT_WHITE);
    cps[5] = new ColorPalette();
    cps[5]->set(COLOR_PRIMARY, (uint16_t) 0x303303);
    cps[5]->set(COLOR_BACKGROUND, (uint16_t) 0x00ff00);
    avatar.setColorPalette(*cps[first_cps]);
    last_rotation_msec = millis();
    M5_LOGI("setup end");
}

uint32_t count = 0;

void loop() {
    M5.update();

#if defined( ARDUINO_M5STACK_CORES3 )
    unifiedButton.update();
#endif

    if (M5.BtnPWR.wasClicked()) {
        // rotate screen 180 degrees
        display_rotation == 1 ? display_rotation = 3 : display_rotation = 1;
        M5.Display.setRotation(display_rotation);
    } else if (M5.BtnA.wasPressed()) {
        M5_LOGI("Push BtnA");
        palette_index++;
        if (palette_index > 5) {
            palette_index = 0;
        }
        avatar.setColorPalette(*cps[palette_index]);
    } else if (M5.BtnB.wasPressed()) {
        if (palette_index == 0) {
            palette_index = 5;
        } else {
            palette_index--;
        }
        avatar.setColorPalette(*cps[palette_index]);
    }

    lipsync();
    vTaskDelay(1 / portTICK_PERIOD_MS);
}
