#include "servo_backend.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mcp_servo, LOG_LEVEL_INF);

#if DT_NODE_HAS_STATUS(DT_ALIAS(servo_pwm), okay)

static const struct pwm_dt_spec servo_pwm = PWM_DT_SPEC_GET(DT_ALIAS(servo_pwm));
static int current_angle = 90;

static int set_angle(servo_backend_t *backend, int angle) {
    uint32_t pulse_us = 500U + ((uint32_t)angle * 2000U) / 180U;
    int result;
    ARG_UNUSED(backend);
    result = pwm_set_dt(&servo_pwm, PWM_MSEC(20), PWM_USEC(pulse_us));
    if (result == 0) current_angle = angle;
    return result == 0 ? MCP_OK : MCP_ERR;
}

static int get_angle(servo_backend_t *backend, int *angle) {
    ARG_UNUSED(backend);
    *angle = current_angle;
    return MCP_OK;
}

int zephyr_servo_backend_init(servo_backend_t *backend) {
    if (!pwm_is_ready_dt(&servo_pwm)) {
        LOG_ERR("Servo PWM device is not ready");
        return MCP_ERR;
    }
    backend->set_angle = set_angle;
    backend->get_angle = get_angle;
    backend->state = NULL;
    LOG_INF("Hardware servo enabled on devicetree alias servo-pwm");
    return set_angle(backend, current_angle);
}

int zephyr_servo_backend_is_hardware(void) { return 1; }

#else

static demo_fake_servo_t fake_servo;

int zephyr_servo_backend_init(servo_backend_t *backend) {
    demo_fake_servo_init(backend, &fake_servo);
    LOG_INF("No servo-pwm alias; using deterministic simulated servo");
    return MCP_OK;
}

int zephyr_servo_backend_is_hardware(void) { return 0; }

#endif
