#pragma once
// =============================================================================
// display_config.h — LovyanGFX Concrete Display Class Definitions
// =============================================================================
// CONFIRMED from skizzophrenic/M5PORKCHOP_DualScreen/src/ui/ext_display.h
//
// CRITICAL ARCHITECTURE (not intuitive):
//   BOTH displays + microSD all share SPI2_HOST (FSPI).
//   They have DIFFERENT MOSI pins (35 vs 14) but SAME SCK (40).
//   LovyanGFX handles this via bus_shared=true + use_lock=true:
//   it reconfigures the MOSI mux per-transaction and serialises access
//   with an internal mutex. This is the only approach proven to work
//   on the Cardputer ADV hardware.
//
//   Do NOT put the ILI9341 on SPI3 — the expansion header's SPI3 pins
//   are not confirmed to be accessible without hardware modification.
//
// LGFX_Primary   — ST7789  internal (240×135), SPI2, MOSI=35
// LGFX_Secondary — ST7789V external (320×240), SPI2, MOSI=14, bus_shared
// =============================================================================

#include <M5GFX.h>
#include "pin_config.h"
#include <lgfx/v1/panel/Panel_ST7789.hpp>
#include <lgfx/v1/panel/Panel_LCD.hpp>

// ─── External screen tuning (ST7789V 240x320 modules) ────────────────────────
// Most 2.0"/2.4"/2.8" ST7789V IPS modules need colour inversion ON.
// If colours look negative (black<->white swapped) set EXT_PANEL_INVERT to false.
// If red and blue are swapped set EXT_PANEL_RGB_ORDER to true.
#ifndef EXT_PANEL_INVERT
#define EXT_PANEL_INVERT     true
#endif
#ifndef EXT_PANEL_RGB_ORDER
#define EXT_PANEL_RGB_ORDER  false
#endif
// SPI write clock for the external panel. 27 MHz is safe over DuPont wires;
// try 40000000 with short wires, drop to 20000000 if you see noise/garbage.
#ifndef EXT_SPI_FREQ
#define EXT_SPI_FREQ         27000000
#endif

namespace lgfx { using namespace lgfx::v1; }

// =============================================================================
// PRIMARY: ST7789 — Internal Cardputer Screen (240×135)
// SPI2 / FSPI, MOSI=35, shared bus with SD card and ILI9341
// =============================================================================
class LGFX_Primary : public lgfx::LGFX_Device {
    lgfx::v1::Panel_ST7789  _panel;
    lgfx::v1::Bus_SPI       _bus;
    lgfx::v1::Light_PWM     _light;

public:
    LGFX_Primary() {
        {
            auto cfg        = _bus.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 40000000;
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = true;        // Write-only
            cfg.use_lock    = true;        // Required: bus shared with SD + ILI9341
            cfg.dma_channel = 0;
            cfg.pin_sclk    = PIN_LCD_SCLK;   // 40
            cfg.pin_mosi    = PIN_LCD_MOSI;   // 35
            cfg.pin_miso    = PIN_LCD_MISO;   // -1
            cfg.pin_dc      = PIN_LCD_DC;     // 33
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg            = _panel.config();
            cfg.pin_cs          = PIN_LCD_CS;     // 37
            cfg.pin_rst         = PIN_LCD_RST;    // 34
            cfg.pin_busy        = -1;
            cfg.memory_width    = 240;
            cfg.memory_height   = 320;
            cfg.panel_width     = LCD_PRIMARY_W;  // 240
            cfg.panel_height    = LCD_PRIMARY_H;  // 135
            cfg.offset_x        = 0;
            cfg.offset_y        = 0;
            cfg.offset_rotation = 1;       // Landscape
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable        = false;
            cfg.invert          = true;    // ST7789 on Cardputer: invert=true
            cfg.rgb_order       = false;
            cfg.dlen_16bit      = false;
            cfg.bus_shared      = true;    // Shares SPI2 with SD + ILI9341
            _panel.config(cfg);
        }
        {
            auto cfg        = _light.config();
            cfg.pin_bl      = PIN_LCD_BL;  // 38
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        setPanel(&_panel);
    }
};

// =============================================================================
// SECONDARY: ST7789V — External 240x320 Screen (320×240 landscape)
// (was ILI9341 in upstream; swapped to the stock LovyanGFX Panel_ST7789)
// SPI2 / FSPI — SAME HOST as primary, bus_shared=true
//
// Key differences from primary:
//   - MOSI = GPIO14 (not 35)
//   - spi_3wire = true (MISO unwired)
//   - dma_channel = 0 (not AUTO — proven stable on shared bus from PorkChop)
//   - freq_write = 27MHz (conservative for DuPont cable runs)
//   - No backlight PWM (LED pin tied to 3.3V)
//   - setRotation(1) called post-init, NOT offset_rotation in panel config
//
// Panel memory is 240×320 (portrait). setRotation(1) yields 320×240 landscape.
// =============================================================================
class LGFX_Secondary : public lgfx::LGFX_Device {
    lgfx::v1::Panel_ST7789  _panel;   // ST7789V 240x320 external screen
    lgfx::v1::Bus_SPI       _bus;

public:
    LGFX_Secondary() {
        {
            auto cfg        = _bus.config();
            cfg.spi_host    = SPI2_HOST;   // SAME host as primary — bus_shared handles arbitration
            cfg.spi_mode    = 0;
            cfg.freq_write  = EXT_SPI_FREQ; // 27MHz default — stable over DuPont wires
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = true;        // MISO unwired
            cfg.use_lock    = true;        // Required for shared bus
            cfg.dma_channel = 0;           // dma_channel=0 per PorkChop: stable on shared SPI2
            cfg.pin_sclk    = PIN_EXT_SCLK;   // 40 — shared clock line
            cfg.pin_mosi    = PIN_EXT_MOSI;   // 14 — ILI9341-only MOSI
            cfg.pin_miso    = PIN_EXT_MISO;   // -1
            cfg.pin_dc      = PIN_EXT_DC;     // 6
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg            = _panel.config();
            cfg.pin_cs          = PIN_EXT_CS;     // 5
            cfg.pin_rst         = PIN_EXT_RST;    // 3
            cfg.pin_busy        = -1;
            // Physical panel + memory: 240×320 (portrait native)
            // setRotation(1) post-init gives us 320×240 landscape
            cfg.memory_width    = 240;
            cfg.memory_height   = 320;
            cfg.panel_width     = 240;
            cfg.panel_height    = 320;
            cfg.offset_x        = 0;
            cfg.offset_y        = 0;
            cfg.offset_rotation = 0;       // Rotation applied via setRotation(1) in Display::init()
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable        = false;   // spi_3wire — no readback
            cfg.invert          = EXT_PANEL_INVERT;     // ST7789V modules: usually true
            cfg.rgb_order       = EXT_PANEL_RGB_ORDER;  // flip if red/blue are swapped
            cfg.dlen_16bit      = false;
            cfg.bus_shared      = true;    // Shares SPI2 with SD + ST7789
            _panel.config(cfg);
        }
        // No Light/backlight object — LED pin tied to 3.3V (always on)
        setPanel(&_panel);
    }
};
