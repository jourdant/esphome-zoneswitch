import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

from .. import ZoneSwitch, zoneswitch_ns

DEPENDENCIES = ["zoneswitch"]
CODEOWNERS = ["@jourdant"]

CONF_ZONESWITCH_ID = "zoneswitch_id"
CONF_METRIC = "metric"

DiagnosticMetric = zoneswitch_ns.enum("DiagnosticMetric")
DIAGNOSTIC_METRICS = {
    "node_address": DiagnosticMetric.DIAGNOSTIC_METRIC_NODE_ADDRESS,
}

ZoneSwitchDiagnosticSensor = zoneswitch_ns.class_(
    "ZoneSwitchDiagnosticSensor", sensor.Sensor, cg.Component
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(ZoneSwitchDiagnosticSensor)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(ZoneSwitchDiagnosticSensor),
            cv.GenerateID(CONF_ZONESWITCH_ID): cv.use_id(ZoneSwitch),
            cv.Required(CONF_METRIC): cv.enum(DIAGNOSTIC_METRICS, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    parent = await cg.get_variable(config[CONF_ZONESWITCH_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_metric(config[CONF_METRIC]))
    cg.add(parent.register_diagnostic(var))
