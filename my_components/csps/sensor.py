"""CSPS 电源传感器平台。

各传感器的寄存器与换算方式与原 custom 集成（csps/main.hpp + KCORES_CSPS 库）
保持一致，键名与原配置中的实体一一对应。
"""
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    ICON_FAN,
    ICON_FLASH,
    ICON_POWER,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_WATT,
)

from . import CONF_CSPS_ID, CspsComponent

DEPENDENCIES = ["csps"]

SENSORS = {
    # 风扇转速（寄存器 0x1E，整数 RPM）
    "fan_speed": sensor.sensor_schema(
        unit_of_measurement="RPM",
        icon=ICON_FAN,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # 温度（寄存器 0x1A / 0x1C，值 / 64 = °C）
    "temp1": sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_THERMOMETER,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "temp2": sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_THERMOMETER,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # 输入电压（寄存器 0x08，值 / 32 = V）
    "voltage_in": sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        icon=ICON_FLASH,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # 输入电流（寄存器 0x0A，值 / 64 = A）
    "current_in": sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        icon=ICON_CURRENT_AC,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # 输入功率（寄存器 0x0C，整数 W）
    "power_in": sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        icon=ICON_POWER,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # 输出电压（寄存器 0x0E，值 / 256 = V）
    "voltage_out": sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        icon=ICON_FLASH,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # 输出电流（寄存器 0x10，值 / 64 = A）
    "current_out": sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        icon=ICON_CURRENT_AC,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # 电源直接上报的输出功率（寄存器 0x12，整数 W）
    "output_power": sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        icon=ICON_POWER,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # 由输出电压 × 输出电流计算得出（与原版 Power_Out 一致，保留两位小数）
    "power_out": sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        icon=ICON_POWER,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # 由输出功率 / 输入功率 × 100 计算得出
    "efficiency": sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        icon="mdi:percent",
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CSPS_ID): cv.use_id(CspsComponent),
        **{cv.Optional(key): schema for key, schema in SENSORS.items()},
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_CSPS_ID])
    for key in SENSORS:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(paren, f"set_{key}_sensor")(sens))
