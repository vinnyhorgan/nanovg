/*
    NanoVG WebGL example using Emscripten.
*/

#include <stdbool.h>
#include <stdio.h>

#define SOKOL_IMPL
#include "sokol_gfx.h"
#include "sokol_log.h"

#define EMSC_GLUE_IMPL
#include "emsc_glue.h"

#include "nanovg.h"
#define SOKOL_NANOVG_IMPL
#include "sokol_nanovg.h"

#include "demo.h"
#include "perf.h"

typedef struct {
    NVGcontext* vg;
    DemoData demo;
    PerfGraph fps;
    double prev_time;
    float mouse_x;
    float mouse_y;
    bool blowup;
    bool initialized;
} app_state_t;

static app_state_t state;

static const sg_pass_action pass_action = {
    .colors[0] = {
        .load_action = SG_LOADACTION_CLEAR,
        .clear_value = { 0.3f, 0.3f, 0.32f, 1.0f },
    },
    .depth = {
        .load_action = SG_LOADACTION_CLEAR,
        .clear_value = 1.0f,
    },
    .stencil = {
        .load_action = SG_LOADACTION_CLEAR,
        .clear_value = 0,
    },
};

// Mouse move callback
static EM_BOOL mouse_move_cb(int event_type, const EmscriptenMouseEvent* e, void* user_data) {
    (void)event_type;
    (void)user_data;
    // targetX/Y gives CSS pixel coordinates relative to the canvas element
    state.mouse_x = (float)e->targetX;
    state.mouse_y = (float)e->targetY;
    return EM_TRUE;
}

// Key callback
static EM_BOOL key_cb(int event_type, const EmscriptenKeyboardEvent* e, void* user_data) {
    (void)user_data;
    if (event_type == EMSCRIPTEN_EVENT_KEYDOWN) {
        if (e->keyCode == 32) { // Space
            state.blowup = !state.blowup;
            return EM_TRUE;
        }
    }
    return EM_FALSE;
}

// Frame callback
static EM_BOOL frame(double time, void* user_data) {
    (void)user_data;

    double curr_time = time / 1000.0;
    double dt = curr_time - state.prev_time;
    state.prev_time = curr_time;

    int fb_width = emsc_width();
    int fb_height = emsc_height();
    float dpi_scale = emsc_dpi_scale();
    float win_width = (float)fb_width / dpi_scale;
    float win_height = (float)fb_height / dpi_scale;

    sg_begin_pass(&(sg_pass){
        .action = pass_action,
        .swapchain = emsc_swapchain(),
    });

    nvgBeginFrame(state.vg, win_width, win_height, dpi_scale);
    renderDemo(state.vg, state.mouse_x, state.mouse_y, win_width, win_height, (float)curr_time, state.blowup, &state.demo);
    renderGraph(state.vg, 5.0f, 5.0f, &state.fps);
    nvgEndFrame(state.vg);

    sg_end_pass();
    sg_commit();

    updateGraph(&state.fps, (float)dt);

    return EM_TRUE;
}

int main(void) {
    // Initialize WebGL context
    emsc_init(&(emsc_desc_t){
        .canvas = "#canvas",
        .sample_count = 4,
    });

    // Setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = emsc_environment(),
        .logger.func = slog_func,
    });

    // Initialize perf graph
    initGraph(&state.fps, GRAPH_RENDER_FPS, "Frame Time");

    // Create NanoVG context
    state.vg = nvgCreateSokol(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (state.vg == NULL) {
        printf("Could not init nanovg.\n");
        return -1;
    }

    // Load demo data
    if (loadDemoData(state.vg, &state.demo) == -1) {
        printf("Could not load demo assets.\n");
        return -1;
    }

    // Setup input callbacks
    emscripten_set_mousemove_callback("#canvas", NULL, EM_TRUE, mouse_move_cb);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_TRUE, key_cb);

    // Start animation loop
    emscripten_request_animation_frame_loop(frame, NULL);

    return 0;
}
