"""CSPS 电源出厂信息文本传感器平台。

信息 ROM（独立 I2C 地址，默认 0x57）中保存的出厂信息，原 custom 集成
仅在启动时打印到日志，本平台将其作为 text_sensor 暴露给 Home Assistant。

寄存器布局（起始地址, 长度），与原 KCORES_CSPS 库一致：
  spare_part_number  备件号    0x12, 10 字节
  manufacture_date   生产日期  0x1D, 8 字节
  manufacturer       制造商    0x2C, 5 字节
  model_name         电源型号  0x32, 26 字节
  option_kit_number  套件号    0x4D, 10 字节
  ct_date_codes      CT 日期码 0x5B, 14 字节
"""
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_CSPS_ID, CspsComponent

DEPENDENCIES = ["csps"]

TEXT_SENSORS = [
    "spare_part_number",
    "manufacture_date",
    "manufacturer",
    "model_name",
    "option_kit_number",
    "ct_date_codes",
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CSPS_ID): cv.use_id(CspsComponent),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            )
            for key in TEXT_SENSORS
        },
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_CSPS_ID])
    for key in TEXT_SENSORS:
        if key in config:
            ts = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(paren, f"set_{key}_text_sensor")(ts))
