"""CSPS 服务器电源 PMBus 组件（hub）。

CSPS（华为/HP/联想等服务器电源）通过私有 PMBus 协议对外提供电压、电流、
功率、温度与风扇转速等信息。本组件按 ESPHome 文档推荐的 external_components
方式实现该协议（非 ESPHome 官方发布），协议部分基于
KCORES-CSPS-to-ATX-Converter 项目的 KCORES_CSPS 库（作者 AlphaArea，GPL
协议），原 ESPHome custom 集成见 hitsword/csps_esphome。
"""
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID

DEPENDENCIES = ["i2c"]
MULTI_CONF = True

csps_ns = cg.esphome_ns.namespace("csps")
CspsComponent = csps_ns.class_("CspsComponent", cg.PollingComponent, i2c.I2CDevice)

CONF_ROM_ADDRESS = "rom_address"
CONF_VERIFY_CHECKSUM = "verify_checksum"
CONF_CSPS_ID = "csps_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CspsComponent),
            # 信息 ROM（出厂信息 EEPROM）的 I2C 地址，默认为主 PMBus 地址 - 8
            cv.Optional(CONF_ROM_ADDRESS): cv.i2c_address,
            # 是否校验 PMBus 返回数据末尾的校验和字节
            cv.Optional(CONF_VERIFY_CHECKSUM, default=True): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x5F))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_ROM_ADDRESS in config:
        cg.add(var.set_rom_address(config[CONF_ROM_ADDRESS]))
    else:
        cg.add(var.set_rom_address(config[CONF_ADDRESS] - 8))
    cg.add(var.set_verify_checksum(config[CONF_VERIFY_CHECKSUM]))
