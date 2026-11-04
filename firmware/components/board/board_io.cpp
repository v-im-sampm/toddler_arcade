#include "little_arcade/board_io.hpp"

#include <initializer_list>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_log.h"
#include "little_arcade/board_pins.hpp"

namespace little_arcade::board {
namespace {

constexpr const char* kTag = "board_io";
constexpr spi_host_device_t kLcdHost = SPI2_HOST;
constexpr int kI2cTimeoutMs = 20;

// NS2009 commands (upstream ui.ino): one command byte, then a 2-byte reply.
constexpr uint8_t kNs2009ReadX = 0xC0;
constexpr uint8_t kNs2009ReadY = 0xD0;
constexpr uint8_t kNs2009ReadZ = 0xE0;

DisplayConfig g_display{};
i2c_master_bus_handle_t g_i2c_bus = nullptr;
i2c_master_dev_handle_t g_touch = nullptr;

bool on_color_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
  if (g_display.on_flush_done) g_display.on_flush_done(g_display.ctx);
  return false;
}

bool pins_known() {
  namespace d = display;
  namespace t = touch;
  for (int pin : {d::kSclk, d::kMosi, d::kCs, d::kDc, d::kRst, d::kBacklight, t::kScl, t::kSda}) {
    if (pin == kPinTbd) return false;
  }
  return true;
}

esp_err_t ns2009_read(uint8_t command, uint16_t* value) {
  uint8_t reply[2] = {};
  esp_err_t err = i2c_master_transmit(g_touch, &command, 1, kI2cTimeoutMs);
  if (err == ESP_OK) err = i2c_master_receive(g_touch, reply, sizeof(reply), kI2cTimeoutMs);
  if (err == ESP_OK) *value = static_cast<uint16_t>((reply[0] << 4) | (reply[1] >> 4));
  return err;
}

}  // namespace

esp_err_t display_init(const DisplayConfig& config, esp_lcd_panel_handle_t* out_panel) {
  if (!pins_known()) return ESP_ERR_INVALID_STATE;
  g_display = config;

  gpio_config_t bl{};
  bl.pin_bit_mask = 1ULL << display::kBacklight;
  bl.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&bl));
  backlight_set(false);

  spi_bus_config_t bus{};
  bus.sclk_io_num = display::kSclk;
  bus.mosi_io_num = display::kMosi;
  bus.miso_io_num = -1;  // upstream example does not read the panel
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = static_cast<int>(config.max_transfer_bytes);
  esp_err_t err = spi_bus_initialize(kLcdHost, &bus, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) return err;

  esp_lcd_panel_io_handle_t io = nullptr;
  esp_lcd_panel_io_spi_config_t io_cfg{};
  io_cfg.cs_gpio_num = display::kCs;
  io_cfg.dc_gpio_num = display::kDc;
  io_cfg.spi_mode = 0;
  io_cfg.pclk_hz = config.pclk_hz;
  io_cfg.trans_queue_depth = 10;
  io_cfg.on_color_trans_done = on_color_done;
  io_cfg.lcd_cmd_bits = 8;
  io_cfg.lcd_param_bits = 8;
  err = esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(kLcdHost), &io_cfg, &io);
  if (err != ESP_OK) return err;

  esp_lcd_panel_dev_config_t panel_cfg{};
  panel_cfg.reset_gpio_num = display::kRst;
  panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;  // upstream TFT_RGB_ORDER TFT_BGR
  panel_cfg.bits_per_pixel = 16;
  esp_lcd_panel_handle_t panel = nullptr;
  err = esp_lcd_new_panel_st7789(io, &panel_cfg, &panel);
  if (err != ESP_OK) return err;

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));  // upstream init sends INVON
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, config.orientation.swap_xy));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, config.orientation.mirror_x, config.orientation.mirror_y));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
  ESP_LOGI(kTag, "ST7789 up: SPI %u Hz, BGR, inverted, swap=%d mirror=%d,%d",
           static_cast<unsigned>(config.pclk_hz), config.orientation.swap_xy, config.orientation.mirror_x,
           config.orientation.mirror_y);
  *out_panel = panel;
  return ESP_OK;
}

void backlight_set(bool on) {
  // Q3 (S8050) low-side switch: high = on per schematic/example; not bench-verified.
  gpio_set_level(static_cast<gpio_num_t>(display::kBacklight), on ? 1 : 0);
}

esp_err_t touch_init() {
  if (!pins_known()) return ESP_ERR_INVALID_STATE;
  i2c_master_bus_config_t bus{};
  bus.i2c_port = I2C_NUM_0;
  bus.sda_io_num = static_cast<gpio_num_t>(touch::kSda);
  bus.scl_io_num = static_cast<gpio_num_t>(touch::kScl);
  bus.clk_source = I2C_CLK_SRC_DEFAULT;
  bus.glitch_ignore_cnt = 7;
  bus.flags.enable_internal_pullup = false;  // board has 3.3 kOhm R11/R12
  esp_err_t err = i2c_new_master_bus(&bus, &g_i2c_bus);
  if (err != ESP_OK) return err;

  i2c_device_config_t dev{};
  dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev.device_address = touch::kI2cAddress;
  dev.scl_speed_hz = 100000;  // upstream Wire1 clock
  err = i2c_master_bus_add_device(g_i2c_bus, &dev, &g_touch);
  if (err != ESP_OK) return err;
  err = i2c_master_probe(g_i2c_bus, touch::kI2cAddress, kI2cTimeoutMs);
  ESP_LOGI(kTag, "NS2009 probe at 0x%02x: %s", touch::kI2cAddress, esp_err_to_name(err));
  return err;
}

esp_err_t touch_read(TouchSample* out, const TouchCalibration& calibration) {
  *out = TouchSample{};
  esp_err_t err = ns2009_read(kNs2009ReadZ, &out->raw_z);
  if (err != ESP_OK || !ns2009_pressed(out->raw_z, calibration)) return err;
  err = ns2009_read(kNs2009ReadX, &out->raw_x);
  if (err == ESP_OK) err = ns2009_read(kNs2009ReadY, &out->raw_y);
  out->pressed = err == ESP_OK;
  return err;
}

}  // namespace little_arcade::board
