#ifndef MCP_ZEPHYR_SERVO_BACKEND_H
#define MCP_ZEPHYR_SERVO_BACKEND_H

#include "demo_tools.h"

/* Uses the servo-pwm devicetree alias when present, otherwise the fake backend. */
int zephyr_servo_backend_init(servo_backend_t *backend);
int zephyr_servo_backend_is_hardware(void);

#endif
