import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import logger, mqtt, number, select, switch, text, uart
from esphome.const import CONF_ID, CONF_VERSION

CODEOWNERS = ["@KrzysztofHajdamowicz"]
DEPENDENCIES = ["uart", "mqtt", "logger", "json"]

CONF_MQTT_ID = "mqtt_id"
CONF_FLOW_CONTROL_PIN = "flow_control_pin"
CONF_ENVIRONMENT = "environment"
CONF_CERTIFICATE_AUTHORITY = "certificate_authority"
CONF_MQTT_HOST_ID = "mqtt_host_id"
CONF_MQTT_PORT_ID = "mqtt_port_id"
CONF_PLANT_ID_ID = "plant_id_id"
CONF_PLANT_TOKEN_ID = "plant_token_id"
CONF_CLOUD_ENABLED_ID = "cloud_enabled_id"
CONF_TLS_ENABLED_ID = "tls_enabled_id"
CONF_TLS_SKIP_CN_CHECK_ID = "tls_skip_cn_check_id"
CONF_BAUD_RATE_ID = "baud_rate_id"
CONF_PARITY_ID = "parity_id"
CONF_RESPONSE_TIMEOUT = "response_timeout"
CONF_READ_GAP = "read_gap"
CONF_WRITE_GAP = "write_gap"
CONF_LOG_BUFFER_SIZE = "log_buffer_size"

gbb_dongle_ns = cg.esphome_ns.namespace("gbb_dongle")
GbbDongle = gbb_dongle_ns.class_("GbbDongle", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GbbDongle),
            cv.GenerateID(CONF_MQTT_ID): cv.use_id(mqtt.MQTTClientComponent),
            cv.Optional(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_VERSION, default="dev"): cv.string_strict,
            cv.Optional(CONF_ENVIRONMENT, default="GbbDongle"): cv.string_strict,
            cv.Optional(CONF_CERTIFICATE_AUTHORITY): cv.string,
            cv.Required(CONF_MQTT_HOST_ID): cv.use_id(text.Text),
            cv.Required(CONF_MQTT_PORT_ID): cv.use_id(number.Number),
            cv.Required(CONF_PLANT_ID_ID): cv.use_id(text.Text),
            cv.Required(CONF_PLANT_TOKEN_ID): cv.use_id(text.Text),
            cv.Required(CONF_CLOUD_ENABLED_ID): cv.use_id(switch.Switch),
            cv.Optional(CONF_TLS_ENABLED_ID): cv.use_id(switch.Switch),
            cv.Optional(CONF_TLS_SKIP_CN_CHECK_ID): cv.use_id(switch.Switch),
            cv.Optional(CONF_BAUD_RATE_ID): cv.use_id(select.Select),
            cv.Optional(CONF_PARITY_ID): cv.use_id(select.Select),
            cv.Optional(
                CONF_RESPONSE_TIMEOUT, default="1000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_READ_GAP, default="100ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_WRITE_GAP, default="3000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_LOG_BUFFER_SIZE, default=65536): cv.positive_int,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    mqtt_client = await cg.get_variable(config[CONF_MQTT_ID])
    cg.add(var.set_mqtt_parent(mqtt_client))

    if CONF_FLOW_CONTROL_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
        cg.add(var.set_flow_control_pin(pin))

    cg.add(var.set_version(config[CONF_VERSION]))
    cg.add(var.set_environment(config[CONF_ENVIRONMENT]))

    if CONF_CERTIFICATE_AUTHORITY in config:
        cg.add(var.set_ca_certificate(config[CONF_CERTIFICATE_AUTHORITY]))
        # Same workaround as the mqtt component applies when TLS is in use,
        # see https://github.com/espressif/esp-idf/issues/139
        from esphome.components.esp32 import add_idf_sdkconfig_option

        add_idf_sdkconfig_option("CONFIG_MBEDTLS_HARDWARE_MPI", False)

    for conf_key, setter in [
        (CONF_MQTT_HOST_ID, var.set_mqtt_host_text),
        (CONF_MQTT_PORT_ID, var.set_mqtt_port_number),
        (CONF_PLANT_ID_ID, var.set_plant_id_text),
        (CONF_PLANT_TOKEN_ID, var.set_plant_token_text),
        (CONF_CLOUD_ENABLED_ID, var.set_cloud_enabled_switch),
        (CONF_TLS_ENABLED_ID, var.set_tls_enabled_switch),
        (CONF_TLS_SKIP_CN_CHECK_ID, var.set_tls_skip_cn_check_switch),
        (CONF_BAUD_RATE_ID, var.set_baud_rate_select),
        (CONF_PARITY_ID, var.set_parity_select),
    ]:
        if conf_key in config:
            entity = await cg.get_variable(config[conf_key])
            cg.add(setter(entity))

    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT]))
    cg.add(var.set_read_gap(config[CONF_READ_GAP]))
    cg.add(var.set_write_gap(config[CONF_WRITE_GAP]))
    cg.add(var.set_log_buffer_size(config[CONF_LOG_BUFFER_SIZE]))

    # Needed so logger::Logger::add_log_callback() compiles to a real
    # implementation (USE_LOG_LISTENERS) for the LastLog ring buffer.
    logger.request_log_listener()
