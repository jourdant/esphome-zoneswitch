import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from .. import ZoneSwitch, zoneswitch_ns

DEPENDENCIES = ["zoneswitch"]
CODEOWNERS = ["@jourdant"]

CONF_ZONESWITCH_ID = "zoneswitch_id"
CONF_METRIC = "metric"

TextSensorMetric = zoneswitch_ns.enum("TextSensorMetric")
TEXT_SENSOR_METRICS = {
    "node_address": TextSensorMetric.TEXT_SENSOR_METRIC_NODE_ADDRESS,
}

ZoneSwitchTextSensor = zoneswitch_ns.class_(
    "ZoneSwitchTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(ZoneSwitchTextSensor)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(ZoneSwitchTextSensor),
            cv.GenerateID(CONF_ZONESWITCH_ID): cv.use_id(ZoneSwitch),
            cv.Required(CONF_METRIC): cv.enum(TEXT_SENSOR_METRICS, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)

    parent = await cg.get_variable(config[CONF_ZONESWITCH_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_metric(config[CONF_METRIC]))
    cg.add(parent.register_diagnostic(var))
