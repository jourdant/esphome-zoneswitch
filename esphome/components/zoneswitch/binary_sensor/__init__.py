import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID

from .. import ZoneSwitch, zoneswitch_ns

DEPENDENCIES = ["zoneswitch"]
CODEOWNERS = ["@jourdant"]

CONF_ZONESWITCH_ID = "zoneswitch_id"
CONF_ZONE = "zone"

ZoneSwitchBinarySensor = zoneswitch_ns.class_(
    "ZoneSwitchBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(ZoneSwitchBinarySensor)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(ZoneSwitchBinarySensor),
            cv.GenerateID(CONF_ZONESWITCH_ID): cv.use_id(ZoneSwitch),
            cv.Required(CONF_ZONE): cv.int_range(min=1, max=6),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    parent = await cg.get_variable(config[CONF_ZONESWITCH_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_zone(config[CONF_ZONE]))
    cg.add(parent.register_zone(var))
