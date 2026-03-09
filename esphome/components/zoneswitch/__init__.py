import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import uart
from esphome.const import CONF_ID
from esphome.cpp_helpers import gpio_pin_expression

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@jourdant"]
MULTI_CONF = True

zoneswitch_ns = cg.esphome_ns.namespace("zoneswitch")
ZoneSwitch = zoneswitch_ns.class_("ZoneSwitch", uart.UARTDevice, cg.Component)

CONF_ZONESWITCH_ID = "zoneswitch_id"
CONF_FLOW_CONTROL_PIN = "flow_control_pin"
CONF_DEBUG = "debug"
CONF_POLL_INTERVAL = "poll_interval"

CONFIG_SCHEMA = (
    uart.UART_DEVICE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ZoneSwitch),
            cv.Optional(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_DEBUG, default=False): cv.boolean,
            cv.Optional(CONF_POLL_INTERVAL, default="5000ms"): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    cg.add_global(zoneswitch_ns.using)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_FLOW_CONTROL_PIN in config:
        pin = await gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
        cg.add(var.set_flow_control_pin(pin))

    cg.add(var.set_debug(config[CONF_DEBUG]))
    cg.add(var.set_poll_interval(config[CONF_POLL_INTERVAL]))
