"""CSPS 电源控制平台（number）。

fan_speed：风扇目标转速（RPM），变更时写入 PMBus 寄存器 0x40。
PS-2751-7H 实测有效范围约 3600–17300 RPM（默认值保持通用，具体范围
在 YAML 中按电源型号标定）；写 0 恢复自动控制请用 button 平台的 fan_auto。
"""
import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    ICON_FAN,
    UNIT_REVOLUTIONS_PER_MINUTE,
)

from . import CONF_CSPS_ID, CspsComponent, csps_ns

DEPENDENCIES = ["csps"]

CONF_FAN_SPEED = "fan_speed"

CspsFanSpeedNumber = csps_ns.class_("CspsFanSpeedNumber", number.Number, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CSPS_ID): cv.use_id(CspsComponent),
        cv.Optional(CONF_FAN_SPEED): number.number_schema(
            CspsFanSpeedNumber,
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            icon=ICON_FAN,
        ).extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=20000): cv.float_,
                cv.Optional(CONF_STEP, default=100): cv.positive_float,
            }
        ),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_CSPS_ID])
    if CONF_FAN_SPEED in config:
        conf = config[CONF_FAN_SPEED]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, config)
        await number.register_number(
            var,
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        cg.add(var.set_parent(paren))
