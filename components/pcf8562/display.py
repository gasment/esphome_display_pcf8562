import hashlib

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, preferences
from esphome.components import display, i2c
from esphome.const import CONF_ID, CONF_LAMBDA
from esphome.core import CORE

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["preferences"]

pcf8562_ns = cg.esphome_ns.namespace("pcf8562")
PCF8562Display = pcf8562_ns.class_("PCF8562Display", cg.PollingComponent, i2c.I2CDevice)
PCF8562DisplayRef = PCF8562Display.operator("ref")

CONF_BIAS = "bias"
CONF_HARDWARE_SUBADDRESS = "hardware_subaddress"

def validate_restore_from_rtc(value):
    value = cv.boolean(value)
    if value:
        if not CORE.is_esp32:
            raise cv.Invalid("restore_from_rtc requires ESP32 deep sleep")
        preferences.validate_rtc_storage(True)
    return value


CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PCF8562Display),
            cv.Optional(CONF_BIAS, default="1/3"): cv.enum({"1/3": 0x00, "1/2": 0x04}),
            cv.Optional(CONF_HARDWARE_SUBADDRESS, default=0): cv.int_range(min=0, max=7),
            cv.Optional("restore_from_rtc", default=False): validate_restore_from_rtc,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x38))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_bias(config[CONF_BIAS]))
    cg.add(var.set_hardware_subaddress(config[CONF_HARDWARE_SUBADDRESS]))
    if config["restore_from_rtc"]:
        preferences.request_rtc_storage()
        key = int(hashlib.sha256(f"pcf8562/frame/v1/{config[CONF_ID].id}".encode()).hexdigest()[:8], 16)
        cg.add(var.set_rtc_storage_key(key))

    if CONF_LAMBDA in config:
        writer = await cg.process_lambda(
            config[CONF_LAMBDA], [(PCF8562DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(writer))

# Register the action alongside the display platform so id resolves to this display.
ContentSetAction = pcf8562_ns.class_("ContentSetAction", automation.Action)

CONTENT_FIELDS = {
    "temperature": (cv.float_, cg.float_),
    "humidity": (cv.float_, cg.float_),
    "wifi": (cv.boolean, cg.bool_),
    "bluetooth": (cv.boolean, cg.bool_),
    "battery_outline": (cv.boolean, cg.bool_),
    "battery_left": (cv.boolean, cg.bool_),
    "battery_middle": (cv.boolean, cg.bool_),
    "battery_right": (cv.boolean, cg.bool_),
    "temperature_icon": (cv.boolean, cg.bool_),
    "humidity_icon": (cv.boolean, cg.bool_),
    "celsius": (cv.boolean, cg.bool_),
    "percent": (cv.boolean, cg.bool_),
}

CONTENT_SET_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(PCF8562Display),
            **{
                cv.Optional(key): cv.templatable(validator)
                for key, (validator, _) in CONTENT_FIELDS.items()
            },
        }
    ),
    cv.has_at_least_one_key(*CONTENT_FIELDS),
)


@automation.register_action(
    "pcf8562.content_set", ContentSetAction, CONTENT_SET_SCHEMA, synchronous=True
)
async def content_set_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    for key, (_, value_type) in CONTENT_FIELDS.items():
        if key in config:
            value = await cg.templatable(config[key], args, value_type)
            cg.add(getattr(var, f"set_{key}")(value))
    return var

