#include "csps.h"

#include "esphome/core/log.h"

namespace esphome::csps {

static const char *const TAG = "csps";

// CSPS PMBus 寄存器及换算（与原 KCORES_CSPS 库一致）
static const uint8_t CSPS_REG_INPUT_VOLTAGE = 0x08;   // 值 / 32 = V
static const uint8_t CSPS_REG_INPUT_CURRENT = 0x0A;   // 值 / 64 = A
static const uint8_t CSPS_REG_INPUT_POWER = 0x0C;     // 值 = W
static const uint8_t CSPS_REG_OUTPUT_VOLTAGE = 0x0E;  // 值 / 256 = V
static const uint8_t CSPS_REG_OUTPUT_CURRENT = 0x10;  // 值 / 64 = A
static const uint8_t CSPS_REG_OUTPUT_POWER = 0x12;    // 值 = W
static const uint8_t CSPS_REG_TEMP1 = 0x1A;           // 值 / 64 = °C
static const uint8_t CSPS_REG_TEMP2 = 0x1C;           // 值 / 64 = °C
static const uint8_t CSPS_REG_FAN_SPEED = 0x1E;       // 值 = RPM
static const uint8_t CSPS_REG_FAN_COMMAND = 0x40;     // 写入风扇目标转速（RPM）

// 读失败时的重试次数（与原库一致）
static const uint8_t CSPS_READ_RETRIES = 3;

bool CspsComponent::read_word_(uint8_t reg, uint16_t *out) {
  // CSPS 私有 PMBus 协议：先写 [寄存器, 校验和] 两个字节（写与读之间有 STOP），
  // 再读回数据。返回数据末尾跟一个校验和字节，三个字节之和 mod 256 为 0
  // 时表示数据完整。
  const uint8_t reg_checksum = (uint8_t) ((0xFF - (reg + (this->address_ << 1))) + 1);
  const uint8_t cmd[2] = {reg, reg_checksum};
  uint8_t data[3] = {0, 0, 0};
  const size_t read_len = this->verify_checksum_ ? 3 : 2;

  for (uint8_t attempt = 0; attempt < CSPS_READ_RETRIES; attempt++) {
    if (this->write(cmd, sizeof(cmd)) != i2c::ERROR_OK) {
      continue;
    }
    if (this->read(data, read_len) != i2c::ERROR_OK) {
      continue;
    }
    if (this->verify_checksum_ && (uint8_t) (data[0] + data[1] + data[2]) != 0) {
      ESP_LOGD(TAG, "Checksum mismatch on register 0x%02X", reg);
      continue;
    }
    *out = data[0] | (data[1] << 8);
    return true;
  }
  return false;
}

bool CspsComponent::write_word_(uint8_t reg, uint16_t value) {
  // CSPS 私有 PMBus 写协议（与原库 writeCSPSword 一致）：
  // 写 [寄存器, 数据低字节, 数据高字节, 校验和] 四个字节，
  // 校验和 = 256 - (从机地址 << 1 + 寄存器 + 数据低字节 + 数据高字节)
  const uint8_t val_lsb = (uint8_t) (value & 0xFF);
  const uint8_t val_msb = (uint8_t) ((value >> 8) & 0xFF);
  const uint8_t reg_checksum =
      (uint8_t) ((0xFF - ((this->address_ << 1) + reg + val_lsb + val_msb)) + 1);
  const uint8_t cmd[4] = {reg, val_lsb, val_msb, reg_checksum};

  for (uint8_t attempt = 0; attempt < CSPS_READ_RETRIES; attempt++) {
    if (this->write(cmd, sizeof(cmd)) == i2c::ERROR_OK) {
      return true;
    }
  }
  return false;
}

bool CspsComponent::read_rom_string_(uint8_t start, uint8_t length, std::string &out) {
  out.clear();
  out.reserve(length);
  for (uint8_t offset = 0; offset < length; offset++) {
    const uint8_t reg = start + offset;
    uint8_t value = 0;
    if (this->rom_device_.write(&reg, 1) != i2c::ERROR_OK) {
      return false;
    }
    if (this->rom_device_.read(&value, 1) != i2c::ERROR_OK) {
      return false;
    }
    out.push_back((char) value);
  }
  // 去掉 ROM 中末尾的填充字符（NUL / 空格）
  while (!out.empty() && (out.back() == '\0' || out.back() == ' ')) {
    out.pop_back();
  }
  return true;
}

void CspsComponent::publish_device_info_() {
  struct RomEntry {
    text_sensor::TextSensor *text_sensor;
    uint8_t start;
    uint8_t length;
  };
  const RomEntry entries[] = {
      {this->spare_part_number_text_sensor_, 0x12, 0x0A},
      {this->manufacture_date_text_sensor_, 0x1D, 0x08},
      {this->manufacturer_text_sensor_, 0x2C, 0x05},
      {this->model_name_text_sensor_, 0x32, 0x1A},
      {this->option_kit_number_text_sensor_, 0x4D, 0x0A},
      {this->ct_date_codes_text_sensor_, 0x5B, 0x0E},
  };

  bool all_ok = true;
  for (const auto &entry : entries) {
    if (entry.text_sensor == nullptr) {
      continue;
    }
    std::string value;
    if (!this->read_rom_string_(entry.start, entry.length, value)) {
      ESP_LOGW(TAG, "Reading ROM at 0x%02X failed", entry.start);
      all_ok = false;
      continue;
    }
    ESP_LOGD(TAG, "ROM 0x%02X: %s", entry.start, value.c_str());
    entry.text_sensor->publish_state(value);
  }
  // 任一 ROM 读取失败时，在后续 update() 中重试
  this->rom_info_published_ = all_ok;
}

void CspsComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CSPS power supply...");
  this->rom_device_.set_i2c_bus(this->bus_);

  this->publish_device_info_();
}

void CspsComponent::update() {
  if (!this->rom_info_published_) {
    this->publish_device_info_();
  }

  uint16_t v_in = 0, i_in = 0, p_in = 0;
  uint16_t v_out = 0, i_out = 0, p_out_reg = 0;
  uint16_t temp1 = 0, temp2 = 0, fan = 0;

  const bool has_v_in = this->read_word_(CSPS_REG_INPUT_VOLTAGE, &v_in);
  const bool has_i_in = this->read_word_(CSPS_REG_INPUT_CURRENT, &i_in);
  const bool has_p_in = this->read_word_(CSPS_REG_INPUT_POWER, &p_in);
  const bool has_v_out = this->read_word_(CSPS_REG_OUTPUT_VOLTAGE, &v_out);
  const bool has_i_out = this->read_word_(CSPS_REG_OUTPUT_CURRENT, &i_out);
  const bool has_p_out_reg = this->read_word_(CSPS_REG_OUTPUT_POWER, &p_out_reg);
  const bool has_temp1 = this->read_word_(CSPS_REG_TEMP1, &temp1);
  const bool has_temp2 = this->read_word_(CSPS_REG_TEMP2, &temp2);
  const bool has_fan = this->read_word_(CSPS_REG_FAN_SPEED, &fan);

  if (has_v_in && has_i_in && has_p_in && has_v_out && has_i_out && has_p_out_reg &&
      has_temp1 && has_temp2 && has_fan) {
    this->status_clear_warning();
  } else {
    this->status_set_warning(LOG_STR("CSPS PMBus read failed"));
  }

  if (has_v_in && this->voltage_in_sensor_ != nullptr) {
    this->voltage_in_sensor_->publish_state(v_in / 32.0f);
  }
  if (has_i_in && this->current_in_sensor_ != nullptr) {
    this->current_in_sensor_->publish_state(i_in / 64.0f);
  }
  if (has_p_in && this->power_in_sensor_ != nullptr) {
    this->power_in_sensor_->publish_state((float) p_in);
  }
  if (has_v_out && this->voltage_out_sensor_ != nullptr) {
    this->voltage_out_sensor_->publish_state(v_out / 256.0f);
  }
  if (has_i_out && this->current_out_sensor_ != nullptr) {
    this->current_out_sensor_->publish_state(i_out / 64.0f);
  }
  if (has_p_out_reg && this->output_power_sensor_ != nullptr) {
    this->output_power_sensor_->publish_state((float) p_out_reg);
  }
  if (has_temp1 && this->temp1_sensor_ != nullptr) {
    this->temp1_sensor_->publish_state(temp1 / 64.0f);
  }
  if (has_temp2 && this->temp2_sensor_ != nullptr) {
    this->temp2_sensor_->publish_state(temp2 / 64.0f);
  }
  if (has_fan && this->fan_speed_sensor_ != nullptr) {
    this->fan_speed_sensor_->publish_state((float) fan);
  }

  // 输出功率 = 输出电压 × 输出电流（与原版一致，保留两位小数）
  // 效率 = 输出功率 / 输入功率 × 100（输入功率为 0 时跳过，避免除零）
  if (has_v_out && has_i_out) {
    const float power_out = (v_out / 256.0f) * (i_out / 64.0f);
    if (this->power_out_sensor_ != nullptr) {
      this->power_out_sensor_->publish_state(power_out);
    }
    if (this->efficiency_sensor_ != nullptr && has_p_in && p_in > 0) {
      this->efficiency_sensor_->publish_state(power_out / (float) p_in * 100.0f);
    }
  }
}

void CspsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CSPS Power Supply:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  ROM Address: 0x%02X", this->rom_device_.get_i2c_address());
  ESP_LOGCONFIG(TAG, "  Verify Checksum: %s", YESNO(this->verify_checksum_));
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Fan Speed", this->fan_speed_sensor_);
  LOG_SENSOR("  ", "Temp 1", this->temp1_sensor_);
  LOG_SENSOR("  ", "Temp 2", this->temp2_sensor_);
  LOG_SENSOR("  ", "Input Voltage", this->voltage_in_sensor_);
  LOG_SENSOR("  ", "Input Current", this->current_in_sensor_);
  LOG_SENSOR("  ", "Input Power", this->power_in_sensor_);
  LOG_SENSOR("  ", "Output Voltage", this->voltage_out_sensor_);
  LOG_SENSOR("  ", "Output Current", this->current_out_sensor_);
  LOG_SENSOR("  ", "Output Power", this->output_power_sensor_);
  LOG_SENSOR("  ", "Calculated Power Out", this->power_out_sensor_);
  LOG_SENSOR("  ", "Efficiency", this->efficiency_sensor_);
  LOG_TEXT_SENSOR("  ", "Spare Part Number", this->spare_part_number_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Manufacture Date", this->manufacture_date_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Manufacturer", this->manufacturer_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Model Name", this->model_name_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Option Kit Number", this->option_kit_number_text_sensor_);
  LOG_TEXT_SENSOR("  ", "CT Date Codes", this->ct_date_codes_text_sensor_);
}

bool CspsComponent::write_fan_speed_command(uint16_t rpm) {
  if (this->write_word_(CSPS_REG_FAN_COMMAND, rpm)) {
    ESP_LOGI(TAG, "Fan speed command written: %u RPM", rpm);
    this->status_clear_warning();
    return true;
  }
  ESP_LOGW(TAG, "Writing fan speed command (%u RPM) failed", rpm);
  this->status_set_warning(LOG_STR("CSPS PMBus write failed"));
  return false;
}

void CspsFanSpeedNumber::control(float value) {
  const uint16_t rpm = (uint16_t) value;
  if (this->parent_->write_fan_speed_command(rpm)) {
    this->publish_state(value);
  }
}

void CspsFanAutoButton::press_action() {
  // 写入 0 恢复电源的自动风扇控制（已在 PS-2751-7H 上实测确认）
  this->parent_->write_fan_speed_command(0);
}

}  // namespace esphome::csps
