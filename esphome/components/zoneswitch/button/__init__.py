import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import button

from .. import CONF_ZONESWITCH_ID, ZoneSwitch, zoneswitch_ns

DEPENDENCIES = ["zoneswitch"]
ZoneSwitchRefreshButton = zoneswitch_ns.class_("ZoneSwitchRefreshButton", button.Button)
CONFIG_SCHEMA = button.button_schema(ZoneSwitchRefreshButton).extend(
    {cv.GenerateID(CONF_ZONESWITCH_ID): cv.use_id(ZoneSwitch)}
)


async def to_code(config):
    var = await button.new_button(config)
    parent = await cg.get_variable(config[CONF_ZONESWITCH_ID])
    cg.add(var.set_parent(parent))
