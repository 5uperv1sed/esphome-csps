"""CSPS 电源控制平台（button）。

fan_auto：向 PMBus 寄存器 0x40 写入 0，恢复电源的自动风扇控制
（已在 PS-2751-7H 上实测确认生效）。
"""
import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import CONF_CSPS_ID, CspsComponent, csps_ns

DEPENDENCIES = ["csps"]

CONF_FAN_AUTO = "fan_auto"

CspsFanAutoButton = csps_ns.class_("CspsFanAutoButton", button.Button, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CSPS_ID): cv.use_id(CspsComponent),
        cv.Optional(CONF_FAN_AUTO): button.button_schema(
            CspsFanAutoButton, icon="mdi:fan-auto"
        ),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_CSPS_ID])
    if CONF_FAN_AUTO in config:
        conf = config[CONF_FAN_AUTO]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, config)
        await button.register_button(var, conf)
        cg.add(var.set_parent(paren))
