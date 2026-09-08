#include "demo_tools.h"

#include <stdio.h>
#include <string.h>

static int fake_set_angle(servo_backend_t *backend, int angle) {
    demo_fake_servo_t *state = (demo_fake_servo_t *)backend->state;
    state->angle = angle;
    return MCP_OK;
}

static int fake_get_angle(servo_backend_t *backend, int *angle) {
    demo_fake_servo_t *state = (demo_fake_servo_t *)backend->state;
    *angle = state->angle;
    return MCP_OK;
}

void demo_fake_servo_init(servo_backend_t *backend, demo_fake_servo_t *state) {
    state->angle = 90;
    backend->set_angle = fake_set_angle;
    backend->get_angle = fake_get_angle;
    backend->state = state;
}

static int echo_tool(const mcp_request_ctx_t *ctx, const mcp_json_value_t *arguments,
                     mcp_json_writer_t *result, void *tool_data) {
    mcp_json_value_t text;
    char decoded[2048];
    (void)ctx;
    (void)tool_data;
    if (mcp_json_object_get(arguments, "text", &text) != MCP_OK ||
        mcp_json_get_string(&text, decoded, sizeof(decoded)) != MCP_OK) return MCP_ERR;
    return mcp_tool_result_text(result, decoded, 0);
}

static int sensor_tool(const mcp_request_ctx_t *ctx, const mcp_json_value_t *arguments,
                       mcp_json_writer_t *result, void *tool_data) {
    (void)ctx;
    (void)arguments;
    (void)tool_data;
    return mcp_tool_result_number(result, "Simulated temperature: 23 C", 23, 0);
}

static int servo_set_tool(const mcp_request_ctx_t *ctx, const mcp_json_value_t *arguments,
                          mcp_json_writer_t *result, void *tool_data) {
    servo_backend_t *servo = (servo_backend_t *)tool_data;
    mcp_json_value_t value;
    char text[48];
    int angle;
    (void)ctx;
    if (mcp_json_object_get(arguments, "angle", &value) != MCP_OK ||
        mcp_json_get_int(&value, &angle) != MCP_OK || angle < 0 || angle > 180 ||
        servo->set_angle(servo, angle) != MCP_OK) return MCP_ERR;
    snprintf(text, sizeof(text), "Servo angle set to %d degrees", angle);
    return mcp_tool_result_number(result, text, angle, 0);
}

static int servo_get_tool(const mcp_request_ctx_t *ctx, const mcp_json_value_t *arguments,
                          mcp_json_writer_t *result, void *tool_data) {
    servo_backend_t *servo = (servo_backend_t *)tool_data;
    char text[48];
    int angle;
    (void)ctx;
    (void)arguments;
    if (servo->get_angle(servo, &angle) != MCP_OK) return MCP_ERR;
    snprintf(text, sizeof(text), "Servo angle is %d degrees", angle);
    return mcp_tool_result_number(result, text, angle, 0);
}

int demo_register_tools(mcp_server_t *server, servo_backend_t *servo) {
    static const mcp_tool_t echo = {
        .name = "echo",
        .title = "Echo text",
        .description = "Return the supplied UTF-8 text unchanged.",
        .input_schema_json =
            "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},"
            "\"required\":[\"text\"],\"additionalProperties\":false}",
        .handler = echo_tool,
    };
    static const mcp_tool_t sensor = {
        .name = "sensor.read_temperature",
        .title = "Read temperature",
        .description = "Read the simulated temperature sensor in degrees Celsius.",
        .input_schema_json = "{\"type\":\"object\",\"additionalProperties\":false}",
        .output_schema_json =
            "{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},"
            "\"required\":[\"value\"]}",
        .handler = sensor_tool,
    };
    mcp_tool_t servo_set = {
        .name = "servo.set_angle",
        .title = "Set servo angle",
        .description = "Set the servo angle from 0 to 180 degrees.",
        .input_schema_json =
            "{\"type\":\"object\",\"properties\":{\"angle\":{\"type\":\"integer\","
            "\"minimum\":0,\"maximum\":180}},\"required\":[\"angle\"],"
            "\"additionalProperties\":false}",
        .output_schema_json =
            "{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},"
            "\"required\":[\"value\"]}",
        .handler = servo_set_tool,
        .tool_data = servo,
    };
    mcp_tool_t servo_get = {
        .name = "servo.get_angle",
        .title = "Get servo angle",
        .description = "Read the last commanded servo angle.",
        .input_schema_json = "{\"type\":\"object\",\"additionalProperties\":false}",
        .output_schema_json =
            "{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},"
            "\"required\":[\"value\"]}",
        .handler = servo_get_tool,
        .tool_data = servo,
    };
    return mcp_server_register_tool(server, &echo) ||
           mcp_server_register_tool(server, &sensor) ||
           mcp_server_register_tool(server, &servo_set) ||
           mcp_server_register_tool(server, &servo_get);
}
