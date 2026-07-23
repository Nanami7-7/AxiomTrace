#include "app_model_id.h"
#include "app_pid.h"
#include "app_planner.h"
#include "app_position_control.h"
#include "filter.h"
#include "project_config.h"  /* 滤波器参数宏 */

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;

#define CHECK(name, condition) do {                                      \
    if (!(condition)) {                                                  \
        (void)printf("FAIL: %s (line %d)\n", (name), __LINE__);         \
        failures++;                                                      \
    }                                                                    \
} while (0)

static float angle_error(float actual, float expected)
{
    float error = actual - expected;
    while (error > 180.0f) error -= 360.0f;
    while (error < -180.0f) error += 360.0f;
    return fabsf(error);
}

static void test_filter_stationary(void)
{
    for (int type = 0; type < (int)FILTER_TYPE_COUNT; type++) {
        if (!filter_type_is_enabled((filter_type_t)type)) continue;
        size_t size = filter_get_static_size((filter_type_t)type);
        void *storage = malloc(size);
        CHECK("filter storage", storage != NULL);
        if (!storage) continue;

        filter_t *filter = filter_create_static((filter_type_t)type, storage, size);
        CHECK("static filter create", filter != NULL);
        if (filter) {
            filter_input_t in = {
                .ax = 0.0f, .ay = 0.0f, .az = 1.0f,
                .gx = 0.0f, .gy = 0.0f, .gz = 0.0f, .dt = 0.01f
            };
            filter_output_t out = {0};
            for (int i = 0; i < 1000; i++) filter->update(filter, &in, &out);
            CHECK("stationary output finite", filter_validate_output(&out) == 1);
            CHECK("stationary pitch", fabsf(out.pitch) < 0.5f);
            CHECK("stationary roll", fabsf(out.roll) < 0.5f);
        }
        free(storage);
    }
}

static void test_filter_checked_api(void)
{
    filter_static_storage_t storage;
    CHECK("public static storage size",
          sizeof(storage) >= filter_get_static_size(FILTER_TYPE_KF));
    CHECK("KF is enabled", filter_type_is_enabled(FILTER_TYPE_KF) == 1);
    CHECK("ESKF availability follows configuration",
          filter_type_is_enabled(FILTER_TYPE_ESKF) == FILTER_ENABLE_ESKF);

    filter_t *filter = filter_create_static(FILTER_TYPE_KF,
                                             &storage, sizeof(storage));
    CHECK("checked filter create", filter != NULL);
    if (filter == NULL) return;

    filter_input_t in = {
        .ax = 0.0f, .ay = 0.0f, .az = 1.0f,
        .gx = 0.0f, .gy = 0.0f, .gz = 0.0f, .dt = 0.01f
    };
    filter_output_t out = {0};
    CHECK("checked update succeeds",
          filter_update_checked(filter, &in, &out) == FILTER_OK);
    CHECK("checked update output", filter_validate_output(&out) == 1);

    in.dt = NAN;
    CHECK("checked update rejects invalid dt",
          filter_update_checked(filter, &in, &out) == FILTER_ERR_INVALID_INPUT);
    CHECK("checked parameter rejects invalid value",
          filter_set_param_checked(filter, FILTER_PARAM_KF_Q_ANGLE, NAN) ==
              FILTER_ERR_INVALID_PARAM);
    CHECK("checked parameter rejects unsupported type",
          filter_set_param_checked(filter, FILTER_PARAM_ALPHA, 0.5f) ==
              FILTER_ERR_INVALID_PARAM);
}

static void test_filter_yaw(void)
{
    for (int type = 0; type < (int)FILTER_TYPE_COUNT; type++) {
        if (!filter_type_is_enabled((filter_type_t)type)) continue;
        filter_t *filter = filter_create((filter_type_t)type);
        CHECK("dynamic filter create", filter != NULL);
        if (!filter) continue;
        filter_input_t in = {
            .ax = 0.0f, .ay = 0.0f, .az = 1.0f,
            .gx = 0.0f, .gy = 0.0f, .gz = 90.0f, .dt = 0.01f
        };
        filter_output_t out = {0};
        for (int i = 0; i < 100; i++) filter->update(filter, &in, &out);
        CHECK("yaw integration", angle_error(out.yaw, 90.0f) < 2.0f);
        CHECK("yaw output finite", filter_validate_output(&out) == 1);
        filter_destroy_safe(filter);
    }
}

static void test_eskf_bias_units(void)
{
    if (!filter_type_is_enabled(FILTER_TYPE_ESKF)) return;
    filter_t *filter = filter_create(FILTER_TYPE_ESKF);
    CHECK("ESKF create", filter != NULL);
    if (!filter) return;
    filter_input_t in = {
        .ax = 0.0f, .ay = 0.0f, .az = 1.0f,
        .gx = 1.0f, .gy = 0.0f, .gz = 0.0f, .dt = 0.01f
    };
    filter_output_t out = {0};
    for (int i = 0; i < 1500; i++) filter->update(filter, &in, &out);
    filter_eskf_diag_t diag;
    CHECK("ESKF diagnostics", filter_eskf_get_diag(filter, &diag) == 0);
    CHECK("ESKF bias uses dps", fabsf(diag.bias_x - 1.0f) < 0.15f);
    CHECK("ESKF stationary tilt", fabsf(out.roll) < 1.0f);
    filter_destroy_safe(filter);
}

static float run_incremental_pid(float dt)
{
    app_pid_t pid;
    app_pid_init(&pid, 0.0f, 1.0f, 0.0f,
                 APP_PID_MODE_INCREMENT, -10.0f, 10.0f);
    app_pid_set_setpoint(&pid, 1.0f);
    int steps = (int)(1.0f / dt + 0.5f);
    float out = 0.0f;
    for (int i = 0; i < steps; i++) out = app_pid_compute(&pid, 0.0f, dt);
    return out;
}

static void test_pid(void)
{
    float fast = run_incremental_pid(0.01f);
    float slow = run_incremental_pid(0.02f);
    CHECK("incremental PID time scaling", fabsf(fast - slow) < 0.03f);
    CHECK("incremental PID integral", fabsf(fast - 1.0f) < 0.03f);

    app_pid_t pid;
    app_pid_init(&pid, 1.0f, 0.0f, 0.0f,
                 APP_PID_MODE_INCREMENT, -10.0f, 10.0f);
    app_pid_set_setpoint(&pid, 2.0f);
    float valid = app_pid_compute(&pid, 0.0f, 0.01f);
    float held = app_pid_compute(&pid, 1.0f, NAN);
    CHECK("invalid PID dt holds output", held == valid);
}

static void test_planner_direction(float target)
{
    app_planner_t planner;
    app_planner_init(&planner, 100.0f, -200.0f, 200.0f);
    app_planner_set_done_threshold(&planner, 0.1f);
    app_planner_start(&planner, target, 50.0f);
    for (int i = 0; i < 1000 && !app_planner_is_done(&planner); i++) {
        (void)app_planner_update(&planner, 0.01f);
    }
    CHECK("planner completes", app_planner_is_done(&planner));
    CHECK("planner target", fabsf(app_planner_get_pos(&planner) - target) < 0.11f);
}

static void test_planner(void)
{
    test_planner_direction(100.0f);
    test_planner_direction(-100.0f);

    app_planner_t planner;
    app_planner_init(&planner, 10.0f, -20.0f, 20.0f);
    app_planner_start(&planner, 100.0f, 0.0f);
    CHECK("zero-speed plan rejected", app_planner_get_state(&planner) == APP_PLANNER_STATE_IDLE);
}

static void test_position_wrap(void)
{
    app_position_ctrl_t ctrl;
    float rpm[APP_POS_MOTOR_COUNT] = {0};
    app_posctrl_init(&ctrl, 0.0f, 0.0f, 0.0f,
                     1.0f, 0.0f, 0.0f,
                     100.0f, 500.0f, 1000U);
    app_posctrl_set_mode(&ctrl, APP_CTRL_MODE_ANGLE, rpm, 0U);
    app_posctrl_start_angle(&ctrl, 2.0f, 100.0f, 179.0f);
    const app_pos_output_t *out = app_posctrl_update(&ctrl, 0.0f, 179.0f,
                                                      0.02f, 1000U);
    CHECK("yaw wrap shortest direction", out && out->yaw_correction > 0.0f &&
                                             out->yaw_correction < 10.0f);
}

static void test_model_identification(void)
{
    enum { COUNT = 200 };
    float rpm[COUNT];
    const float steady = 1000.0f;
    const float tau = 0.25f;
    const float dt = 0.01f;
    for (int i = 0; i < COUNT; i++)
        rpm[i] = steady * (1.0f - expf(-(float)i * dt / tau));

    app_id_step_result_t result;
    CHECK("model identification", app_id_process_step(rpm, COUNT, 500, dt, &result));
    CHECK("identified time constant", fabsf(result.T_s - tau) < 0.03f);
    CHECK("identified gain", fabsf(result.K - 2.0f) < 0.05f);
    CHECK("model fit", result.fit_quality > 0.95f);
}

int main(void)
{
    test_filter_stationary();
    test_filter_checked_api();
    test_filter_yaw();
    test_eskf_bias_units();
    test_pid();
    test_planner();
    test_position_wrap();
    test_model_identification();

    if (failures == 0) {
        (void)printf("algorithm regression: PASS\n");
        return 0;
    }
    (void)printf("algorithm regression: %d failure(s)\n", failures);
    return 1;
}
