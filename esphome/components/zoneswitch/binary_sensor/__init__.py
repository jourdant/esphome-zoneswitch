import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID

from .. import ZoneSwitch, zoneswitch_ns

DEPENDENCIES = ["zoneswitch"]
CODEOWNERS = ["@jourdant"]

CONF_ZONESWITCH_ID = "zoneswitch_id"
CONF_ZONE = "zone"
CONF_METRIC = "metric"

BinarySensorMetric = zoneswitch_ns.enum("BinarySensorMetric")
BINARY_SENSOR_METRICS = {
    "online": BinarySensorMetric.BINARY_SENSOR_METRIC_ONLINE,
}

ZoneSwitchBinarySensor = zoneswitch_ns.class_(
    "ZoneSwitchBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(ZoneSwitchBinarySensor)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(ZoneSwitchBinarySensor),
            cv.GenerateID(CONF_ZONESWITCH_ID): cv.use_id(ZoneSwitch),
            cv.Optional(CONF_ZONE): cv.int_range(min=1, max=6),
            cv.Optional(CONF_METRIC): cv.enum(BINARY_SENSOR_METRICS, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def _validate_binary_sensor_config(config):
    has_zone = CONF_ZONE in config
    has_metric = CONF_METRIC in config

    if has_zone == has_metric:
        raise cv.Invalid("Exactly one of 'zone' or 'metric' must be set for zoneswitch binary_sensor")

    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate_binary_sensor_config)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    parent = await cg.get_variable(config[CONF_ZONESWITCH_ID])
    cg.add(var.set_parent(parent))

    if CONF_ZONE in config:
        cg.add(var.set_zone(config[CONF_ZONE]))
        cg.add(var.set_type(BinarySensorMetric.BINARY_SENSOR_METRIC_ZONE))
        cg.add(parent.register_zone(var))
    else:
        cg.add(var.set_type(config[CONF_METRIC]))
        cg.add(parent.register_diagnostic(var))
