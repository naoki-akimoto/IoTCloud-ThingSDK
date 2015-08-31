#include "example.h"

#include <kii_iot.h>
#include <kii_iot_application.h>
#include <kii_json.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <kii_core_secure_socket.h>
#include <kii_socket_impl.h>
#include <kii_task_impl.h>

typedef struct prv_smartlight_t {
    kii_json_boolean_t power;
    int brightness;
    int color[3];
    int color_temperature;
} prv_smartlight_t;

static prv_smartlight_t m_smartlight;

static void logger_cb(const char* format, ...)
{
    va_list list;
    va_start(list, format);
    vprintf(format, list);
    va_end(list);
}

static prv_json_read_object(
        const char* json,
        size_t json_len,
        kii_json_field_t* fields,
        char error[EMESSAGE_SIZE + 1])
{
    kii_json_t kii_json;
    kii_json_resource_t* resource_pointer = NULL;
#ifndef KII_JSON_FIELD_TYPE_BOOLEAN
    kii_json_resource_t resource;
    kii_json_token_t tokens[32];
    resource_pointer = &resource;
    resource.tokens = tokens;
    resource.tokens_num = sizeof(tokens) / sizeof(tokens[0]);
#endif

    memset(&kii_json, 0, sizeof(kii_json));
    kii_json.resource = resource_pointer;
    kii_json.error_string_buff = error;
    kii_json.error_string_length = EMESSAGE_SIZE + 1;

    return kii_json_read_object(&kii_json, json, json_len, fields);
}

static kii_bool_t action_handler(
        const char* schema,
        int schema_version,
        const char* action_name,
        const char* action_params,
        char error[EMESSAGE_SIZE + 1])
{
    printf("schema=%s, schema_version=%d, action name=%s, action params=%s\n",
            schema, schema_version, action_name, action_params);

    if (strcmp(schema, "SmartLightDemo") != 0 && schema_version != 1) {
        printf("invalid schema: %s %d\n", schema, schema_version);
        return KII_FALSE;
    }

    if (strcmp(action_name, "turnPower") == 0) {
        kii_json_field_t fields[2];

        memset(fields, 0x00, sizeof(fields));
        fields[0].path = "/power";
        fields[0].type = KII_JSON_FIELD_TYPE_BOOLEAN;
        fields[1].path = NULL;
        if(prv_json_read_object(action_params, strlen(action_params),
                        fields, error) !=  KII_JSON_PARSE_SUCCESS) {
            printf("invalid turnPower json\n");
            return KII_FALSE;
        }
        m_smartlight.power = fields[0].field_copy.boolean_value;
        return KII_TRUE;
    } else if (strcmp(action_name, "setBrightness") == 0) {
        kii_json_field_t fields[2];

        memset(fields, 0x00, sizeof(fields));
        fields[0].path = "/brightness";
        fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
        fields[1].path = NULL;
        if(prv_json_read_object(action_params, strlen(action_params),
                        fields, error) !=  KII_JSON_PARSE_SUCCESS) {
            printf("invalid brightness json\n");
            return KII_FALSE;
        }
        m_smartlight.brightness = fields[0].field_copy.int_value;
        return KII_TRUE;
    } else if (strcmp(action_name, "setColor") == 0) {
        kii_json_field_t fields[4];

        memset(fields, 0x00, sizeof(fields));
        fields[0].path = "/color/[0]";
        fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
        fields[1].path = "/color/[1]";
        fields[1].type = KII_JSON_FIELD_TYPE_INTEGER;
        fields[2].path = "/color/[2]";
        fields[2].type = KII_JSON_FIELD_TYPE_INTEGER;
        fields[3].path = NULL;
        if(prv_json_read_object(action_params, strlen(action_params),
                         fields, error) !=  KII_JSON_PARSE_SUCCESS) {
            printf("invalid color json\n");
            return KII_FALSE;
        }
        m_smartlight.color[0] = fields[0].field_copy.int_value;
        m_smartlight.color[1] = fields[1].field_copy.int_value;
        m_smartlight.color[2] = fields[2].field_copy.int_value;
        return KII_TRUE;
    } else if (strcmp(action_name, "setColorTemperature") == 0) {
        kii_json_field_t fields[2];

        memset(fields, 0x00, sizeof(fields));
        fields[0].path = "/colorTemperature";
        fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
        fields[1].path = NULL;
        if(prv_json_read_object(action_params, strlen(action_params),
                        fields, error) !=  KII_JSON_PARSE_SUCCESS) {
            printf("invalid colorTemperature json\n");
            return KII_FALSE;
        }
        m_smartlight.color_temperature = fields[0].field_copy.int_value;
        return KII_TRUE;
    } else {
        printf("invalid action: %s\n", action_name);
        return KII_FALSE;
    }
}

static kii_bool_t state_handler(
        kii_t* kii,
        KII_IOT_WRITER writer)
{
    FILE* fp = fopen("smartlight-state.json", "r");
    if (fp != NULL) {
        char buf[256];
        kii_bool_t retval = KII_TRUE;
        while (fgets(buf, sizeof(buf) / sizeof(buf[0]), fp) != NULL) {
            if ((*writer)(kii, buf) == KII_FALSE) {
                retval = KII_FALSE;
                break;
            }
        }
        fclose(fp);
        return retval;
    } else {
        char buf[256];
        if ((*writer)(kii, "{\"power\":") == KII_FALSE) {
            return KII_FALSE;
        }
        if ((*writer)(kii, m_smartlight.power == KII_JSON_TRUE
                        ? "true," : "false,") == KII_FALSE) {
            return KII_FALSE;
        }
        if ((*writer)(kii, "\"brightness\":") == KII_FALSE) {
            return KII_FALSE;
        }

        sprintf(buf, "%d,", m_smartlight.brightness);
        if ((*writer)(kii, buf) == KII_FALSE) {
            return KII_FALSE;
        }

        if ((*writer)(kii, "\"color\":") == KII_FALSE) {
            return KII_FALSE;
        }
        sprintf(buf, "[%d,%d,%d],", m_smartlight.color[0],
                m_smartlight.color[1], m_smartlight.color[2]);
        if ((*writer)(kii, buf) == KII_FALSE) {
            return KII_FALSE;
        }

        if ((*writer)(kii, "\"colorTemperature\":") == KII_FALSE) {
            return KII_FALSE;
        }
        sprintf(buf, "%d}", m_smartlight.color_temperature);
        if ((*writer)(kii, buf) == KII_FALSE) {
            return KII_FALSE;
        }
        return KII_TRUE;
    }
}

void setup_command_handler_callbacks(kii_t *command_handler)
{
    /* setting http socket callbacks */
    command_handler->kii_core.http_context.connect_cb = s_connect_cb;
    command_handler->kii_core.http_context.send_cb = s_send_cb;
    command_handler->kii_core.http_context.recv_cb = s_recv_cb;
    command_handler->kii_core.http_context.close_cb = s_close_cb;

    /* setting logger callbacks. */
    command_handler->kii_core.logger_cb = logger_cb;

    /* setting mqtt socket callbacks. */
    command_handler->mqtt_socket_connect_cb = connect_cb;
    command_handler->mqtt_socket_send_cb = send_cb;
    command_handler->mqtt_socket_recv_cb = recv_cb;
    command_handler->mqtt_socket_close_cb = close_cb;

    /* setting task callbacks. */
    command_handler->task_create_cb = task_create_cb;
    command_handler->delay_ms_cb = delay_ms_cb;
}

void setup_state_updater_callbacks(kii_t *state_updater)
{
    /* setting http socket callbacks */
    state_updater->kii_core.http_context.connect_cb = s_connect_cb;
    state_updater->kii_core.http_context.send_cb = s_send_cb;
    state_updater->kii_core.http_context.recv_cb = s_recv_cb;
    state_updater->kii_core.http_context.close_cb = s_close_cb;

    /* setting logger callbacks. */
    state_updater->kii_core.logger_cb = logger_cb;

    /* setting task callbacks. */
    state_updater->task_create_cb = task_create_cb;
    state_updater->delay_ms_cb = delay_ms_cb;
}

int main(int argc, char** argv)
{
    kii_iot_command_handler_resource_t command_handler_resource;
    kii_iot_state_updater_resource_t state_updater_resource;
    char command_handler_buff[EX_COMMAND_HANDLER_BUFF_SIZE];
    char state_updater_buff[EX_STATE_UPDATER_BUFF_SIZE];
    char mqtt_buff[EX_MQTT_BUFF_SIZE];
    kii_iot_t kii_iot;

    command_handler_resource.buffer = command_handler_buff;
    command_handler_resource.buffer_size =
        sizeof(command_handler_buff) / sizeof(command_handler_buff[0]);
    command_handler_resource.mqtt_buffer = mqtt_buff;
    command_handler_resource.mqtt_buffer_size =
        sizeof(mqtt_buff) / sizeof(mqtt_buff[0]);
    command_handler_resource.action_handler = action_handler;

    state_updater_resource.buffer = state_updater_buff;
    state_updater_resource.buffer_size =
        sizeof(state_updater_buff) / sizeof(state_updater_buff[0]);
    state_updater_resource.period = EX_STATE_UPDATE_PERIOD;
    state_updater_resource.state_handler = state_handler;

    init_kii_iot(&kii_iot, EX_APP_ID, EX_APP_KEY, EX_APP_SITE,
            &command_handler_resource, &state_updater_resource, NULL);

    onboard_with_vendor_thing_id(&kii_iot, EX_AUTH_VENDOR_ID,
            EX_AUTH_VENDOR_PASS, NULL, NULL);

    while (1) {}
    return 0;
}
