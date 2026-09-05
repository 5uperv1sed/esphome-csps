#pragma once

#include <string>

#include "esphome/components/button/button.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome::csps {

// CSPS（华为/HP/联想等服务器电源）PMBus 读取组件。
//
// CSPS 的 PMBus 为私有协议：写寄存器时须在命令字节后附校验和字节，
// 读回的数据末尾也带一个校验和字节。协议实现基于
// KCORES-CSPS-to-ATX-Converter 项目的 KCORES_CSPS 库（作者 AlphaArea，GPL），
// 原 ESPHome custom 集成见 hitsword/csps_esphome；本组件为第三版，
// 按 ESPHome 文档推荐的 external_components 方式改写。
class CspsComponent final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_rom_address(uint8_t address) { this->rom_device_.set_i2c_address(address); }
  void set_verify_checksum(bool verify_checksum) { this->verify_checksum_ = verify_checksum; }

  void set_fan_speed_sensor(sensor::Sensor *s) { this->fan_speed_sensor_ = s; }
  void set_temp1_sensor(sensor::Sensor *s) { this->temp1_sensor_ = s; }
  void set_temp2_sensor(sensor::Sensor *s) { this->temp2_sensor_ = s; }
  void set_voltage_in_sensor(sensor::Sensor *s) { this->voltage_in_sensor_ = s; }
  void set_current_in_sensor(sensor::Sensor *s) { this->current_in_sensor_ = s; }
  void set_power_in_sensor(sensor::Sensor *s) { this->power_in_sensor_ = s; }
  void set_voltage_out_sensor(sensor::Sensor *s) { this->voltage_out_sensor_ = s; }
  void set_current_out_sensor(sensor::Sensor *s) { this->current_out_sensor_ = s; }
  void set_output_power_sensor(sensor::Sensor *s) { this->output_power_sensor_ = s; }
  void set_power_out_sensor(sensor::Sensor *s) { this->power_out_sensor_ = s; }
  void set_efficiency_sensor(sensor::Sensor *s) { this->efficiency_sensor_ = s; }

  void set_spare_part_number_text_sensor(text_sensor::TextSensor *ts) {
    this->spare_part_number_text_sensor_ = ts;
  }
  void set_manufacture_date_text_sensor(text_sensor::TextSensor *ts) {
    this->manufacture_date_text_sensor_ = ts;
  }
  void set_manufacturer_text_sensor(text_sensor::TextSensor *ts) {
    this->manufacturer_text_sensor_ = ts;
  }
  void set_model_name_text_sensor(text_sensor::TextSensor *ts) {
    this->model_name_text_sensor_ = ts;
  }
  void set_option_kit_number_text_sensor(text_sensor::TextSensor *ts) {
    this->option_kit_number_text_sensor_ = ts;
  }
  void set_ct_date_codes_text_sensor(text_sensor::TextSensor *ts) {
    this->ct_date_codes_text_sensor_ = ts;
  }

  // 写风扇目标转速（寄存器 0x40）。写入 0 恢复电源自动风扇控制
  // （已在 PS-2751-7H 上实测确认；PS-2751-7H 有效转速范围约 3600–17300）。
  bool write_fan_speed_command(uint16_t rpm);

 protected:
  bool read_word_(uint8_t reg, uint16_t *out);
  bool write_word_(uint8_t reg, uint16_t value);
  bool read_rom_string_(uint8_t start, uint8_t length, std::string &out);
  void publish_device_info_();

  // 出厂信息 ROM 占用独立的 I2C 地址（默认为主地址 - 8，即 0x57）
  i2c::I2CDevice rom_device_;
  bool verify_checksum_{true};
  bool rom_info_published_{false};

  sensor::Sensor *fan_speed_sensor_{nullptr};
  sensor::Sensor *temp1_sensor_{nullptr};
  sensor::Sensor *temp2_sensor_{nullptr};
  sensor::Sensor *voltage_in_sensor_{nullptr};
  sensor::Sensor *current_in_sensor_{nullptr};
  sensor::Sensor *power_in_sensor_{nullptr};
  sensor::Sensor *voltage_out_sensor_{nullptr};
  sensor::Sensor *current_out_sensor_{nullptr};
  sensor::Sensor *output_power_sensor_{nullptr};
  sensor::Sensor *power_out_sensor_{nullptr};
  sensor::Sensor *efficiency_sensor_{nullptr};

  text_sensor::TextSensor *spare_part_number_text_sensor_{nullptr};
  text_sensor::TextSensor *manufacture_date_text_sensor_{nullptr};
  text_sensor::TextSensor *manufacturer_text_sensor_{nullptr};
  text_sensor::TextSensor *model_name_text_sensor_{nullptr};
  text_sensor::TextSensor *option_kit_number_text_sensor_{nullptr};
  text_sensor::TextSensor *ct_date_codes_text_sensor_{nullptr};
};

// 风扇目标转速（number 实体），变更时写入寄存器 0x40
class CspsFanSpeedNumber : public number::Number, public Component {
 public:
  void set_parent(CspsComponent *parent) { this->parent_ = parent; }
  void control(float value) override;

 protected:
  CspsComponent *parent_;
};

// 恢复风扇自动控制（按钮），向寄存器 0x40 写入 0
class CspsFanAutoButton : public button::Button, public Component {
 public:
  void set_parent(CspsComponent *parent) { this->parent_ = parent; }
  void press_action() override;

 protected:
  CspsComponent *parent_;
};

}  // namespace esphome::csps
