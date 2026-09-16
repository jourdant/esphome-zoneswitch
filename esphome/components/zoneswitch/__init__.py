import zlib

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.const import CONF_ID, CONF_UART_ID
from esphome.core import CORE
from esphome.cpp_helpers import gpio_pin_expression

from esphome import pins
from esphome.components import uart

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@jourdant"]
MULTI_CONF = True

zoneswitch_ns = cg.esphome_ns.namespace("zoneswitch")
ZoneSwitch = zoneswitch_ns.class_("ZoneSwitch", uart.UARTDevice, cg.Component)

Protocol = zoneswitch_ns.enum("Protocol", is_class=True)
PROTOCOLS = {"v1": Protocol.V1, "v2": Protocol.V2}
CONF_PROTOCOL = "protocol"

CONF_ZONESWITCH_ID = "zoneswitch_id"
CONF_FLOW_CONTROL_PIN = "flow_control_pin"
CONF_DEBUG = "debug"
CONF_POLL_INTERVAL = "poll_interval"
CONF_TX_NODE_ADDR = "tx_node_addr"
CONF_ENABLE_POLLING = "enable_polling"
CONF_OFFLINE_MISS_THRESHOLD = "offline_miss_threshold"
CONF_SPILL_ZONE = "spill_zone"
CONF_TX_IDLE_GUARD = "tx_idle_guard"
CONF_NODE_CONFIRMATIONS = "node_confirmations"
CONF_NODE_MISMATCH_THRESHOLD = "node_mismatch_threshold"
CONF_RESTORE_NODE = "restore_node"
CONF_STATUS_TIMEOUT = "status_timeout"
CONF_DIAGNOSTIC_UPDATE_INTERVAL = "diagnostic_update_interval"


def _validate_poll_interval(value):
    value = cv.positive_time_period_milliseconds(value)
    total_milliseconds = value.total_milliseconds
    if callable(total_milliseconds):
        total_milliseconds = total_milliseconds()
    if total_milliseconds < 500:
        raise cv.Invalid("poll_interval must be at least 500ms")
    return value


def _validate_bounded_milliseconds(value):
    value = cv.positive_time_period_milliseconds(value)
    total_milliseconds = value.total_milliseconds
    if callable(total_milliseconds):
        total_milliseconds = total_milliseconds()
    if total_milliseconds >= 0x80000000:
        raise cv.Invalid("duration must be less than 24.8 days")
    return value


CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ZoneSwitch),
        cv.Optional(CONF_PROTOCOL, default="v2"): cv.one_of("v1", "v2", lower=True),
        cv.Optional(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_DEBUG, default=False): cv.boolean,
        cv.Optional(CONF_POLL_INTERVAL, default="5000ms"): _validate_poll_interval,
        cv.Optional(CONF_TX_NODE_ADDR, default=0x48): cv.int_range(min=0, max=255),
        cv.Optional(CONF_ENABLE_POLLING): cv.boolean,
        cv.Optional(CONF_OFFLINE_MISS_THRESHOLD, default=5): cv.int_range(
            min=1, max=255
        ),
        cv.Optional(CONF_SPILL_ZONE, default=0): cv.int_range(min=0, max=6),
        cv.Optional(
            CONF_TX_IDLE_GUARD, default="20ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_NODE_CONFIRMATIONS, default=3): cv.int_range(min=1, max=10),
        cv.Optional(CONF_NODE_MISMATCH_THRESHOLD, default=5): cv.int_range(
            min=1, max=255
        ),
        cv.Optional(CONF_RESTORE_NODE, default=False): cv.boolean,
        cv.Optional(CONF_STATUS_TIMEOUT): _validate_bounded_milliseconds,
        cv.Optional(
            CONF_DIAGNOSTIC_UPDATE_INTERVAL, default="10s"
        ): _validate_bounded_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


def _protocol_defaults(config):
    v1 = config[CONF_PROTOCOL] == "v1"
    config.setdefault(CONF_ENABLE_POLLING, not v1)
    config.setdefault(
        CONF_STATUS_TIMEOUT, _validate_bounded_milliseconds("0s" if v1 else "30s")
    )
    if v1:
        if not CORE.is_esp32 or CORE.target_framework != "esp-idf":
            raise cv.Invalid(
                "protocol v1 requires ESP32-S3 with framework type esp-idf"
            )
        if CONF_FLOW_CONTROL_PIN not in config:
            raise cv.Invalid(
                "protocol v1 requires zoneswitch.flow_control_pin (not uart.flow_control_pin)"
            )
        pin = config[CONF_FLOW_CONTROL_PIN]
        if any(
            key
            not in (
                "id",
                "number",
                "mode",
                "inverted",
                "ignore_strapping_warning",
                "ignore_pin_validation_error",
                "allow_other_uses",
                "drive_strength",
            )
            for key in pin
        ):
            raise cv.Invalid("V1 direction pin must be an internal ESP32 GPIO")
        if pin.get("inverted") or pin.get("mode", {}).get("open_drain"):
            raise cv.Invalid("V1 direction pin must be non-inverted and push-pull")
        if config[CONF_RESTORE_NODE]:
            raise cv.Invalid("restore_node applies only to protocol v2")
    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _protocol_defaults)


def _final_validate(config):
    v1 = config[CONF_PROTOCOL] == "v1"
    uart.final_validate_device_schema(
        "zoneswitch",
        baud_rate=250000 if v1 else 9600,
        require_rx=True,
        require_tx=True,
        data_bits=8,
        parity="NONE",
        stop_bits=1,
    )(config)
    full = fv.full_config.get()
    path = full.get_path_for_id(config[CONF_UART_ID])[:-1]
    bus = full.get_config_for_path(path)
    if CONF_FLOW_CONTROL_PIN in bus and CONF_FLOW_CONTROL_PIN in config:
        raise cv.Invalid(
            "Configure only one owner of RS485 direction: UART or ZoneSwitch"
        )
    if bus.get("debug", {}).get("dummy_receiver", False):
        raise cv.Invalid("zoneswitch consumes UART bytes; set dummy_receiver: false")
    if v1:
        if full["esp32"]["variant"] != "ESP32S3":
            raise cv.Invalid(
                "V1 fast turnaround is currently supported only on ESP32-S3"
            )
        if str(bus[CONF_ID].type) != str(uart.IDFUARTComponent):
            raise cv.Invalid("V1 requires the native ESP32 UART")
        if bus.get("rx_full_threshold") != 1:
            raise cv.Invalid(
                "V1 requires uart.rx_full_threshold: 1 for prompt ACK delivery"
            )
        if any(bus.get(pin, {}).get("inverted", False) for pin in ("rx_pin", "tx_pin")):
            raise cv.Invalid("V1 UART pins must be non-inverted")
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_FLOW_CONTROL_PIN in config:
        pin = await gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
        cg.add(var.set_flow_control_pin(pin))

    cg.add(var.set_protocol(PROTOCOLS[config[CONF_PROTOCOL]]))
    if config[CONF_PROTOCOL] == "v1":
        cg.add_define("USE_ZONESWITCH_V1")
        cg.add(var.set_v1_direction_pin(config[CONF_FLOW_CONTROL_PIN]["number"]))

    cg.add(var.set_debug(config[CONF_DEBUG]))
    cg.add(var.set_poll_interval(config[CONF_POLL_INTERVAL]))
    cg.add(var.set_tx_node_addr(config[CONF_TX_NODE_ADDR]))
    cg.add(var.set_enable_polling(config[CONF_ENABLE_POLLING]))
    cg.add(var.set_offline_miss_threshold(config[CONF_OFFLINE_MISS_THRESHOLD]))
    cg.add(var.set_spill_zone(config[CONF_SPILL_ZONE]))
    cg.add(var.set_tx_idle_guard(config[CONF_TX_IDLE_GUARD]))
    cg.add(var.set_node_confirmations(config[CONF_NODE_CONFIRMATIONS]))
    cg.add(var.set_node_mismatch_threshold(config[CONF_NODE_MISMATCH_THRESHOLD]))
    cg.add(var.set_restore_node(config[CONF_RESTORE_NODE]))
    preference_key = zlib.crc32(f"zoneswitch:{config[CONF_ID].id}".encode())
    cg.add(var.set_preference_key(preference_key))
    cg.add(var.set_status_timeout(config[CONF_STATUS_TIMEOUT]))
    cg.add(var.set_diagnostic_update_interval(config[CONF_DIAGNOSTIC_UPDATE_INTERVAL]))
