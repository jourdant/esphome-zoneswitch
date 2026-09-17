import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from esphome.components import sensor

from .. import ZoneSwitch, validate_diagnostic_metric, zoneswitch_ns

DEPENDENCIES = ["zoneswitch"]
CODEOWNERS = ["@jourdant"]

CONF_ZONESWITCH_ID = "zoneswitch_id"
CONF_METRIC = "metric"

DiagnosticMetric = zoneswitch_ns.enum("DiagnosticMetric")
DIAGNOSTIC_METRICS = {
    "rejected_busy": DiagnosticMetric.DIAGNOSTIC_METRIC_REJECTED_BUSY,
    "response_timeouts": DiagnosticMetric.DIAGNOSTIC_METRIC_RESPONSE_TIMEOUTS,
    "ack_timeouts": DiagnosticMetric.DIAGNOSTIC_METRIC_ACK_TIMEOUTS,
    "tx_count": DiagnosticMetric.DIAGNOSTIC_METRIC_TX_COUNT,
    "status_age": DiagnosticMetric.DIAGNOSTIC_METRIC_STATUS_AGE,
    "node_address": DiagnosticMetric.DIAGNOSTIC_METRIC_NODE_ADDRESS,
    "online": DiagnosticMetric.DIAGNOSTIC_METRIC_ONLINE,
    "rx_ok": DiagnosticMetric.DIAGNOSTIC_METRIC_RX_OK,
    "rx_bad": DiagnosticMetric.DIAGNOSTIC_METRIC_RX_BAD,
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


FINAL_VALIDATE_SCHEMA = validate_diagnostic_metric


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    parent = await cg.get_variable(config[CONF_ZONESWITCH_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_metric(config[CONF_METRIC]))
    cg.add(parent.register_diagnostic(var))
