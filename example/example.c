#include <stdbool.h>
#include <stdio.h>

#define SOKOL_IMPL
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "sokol_log.h"

#include "nanovg.h"
#define SOKOL_NANOVG_IMPL
#include "sokol_nanovg.h"

#include "demo.h"
#include "perf.h"

typedef struct {
    NVGcontext* vg;
    DemoData demo;
    PerfGraph fps;
    PerfGraph cpu;
    double time;
    float mouse_x;
    float mouse_y;
    bool blowup;
    bool ready;
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

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    initGraph(&state.fps, GRAPH_RENDER_FPS, "Frame Time");
    initGraph(&state.cpu, GRAPH_RENDER_MS, "CPU Time");

    state.vg = nvgCreateSokol(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
    if (state.vg == NULL) {
        fprintf(stderr, "Could not init nanovg.\n");
        sapp_request_quit();
        return;
    }

    if (loadDemoData(state.vg, &state.demo) == -1) {
        fprintf(stderr, "Could not load demo assets.\n");
        sapp_request_quit();
        return;
    }

    state.ready = true;
}

static void frame(void) {
    const double dt = sapp_frame_duration();
    const float dpi_scale = sapp_dpi_scale() > 0.0f ? sapp_dpi_scale() : 1.0f;
    const float fb_width = sapp_widthf();
    const float fb_height = sapp_heightf();
    const float win_width = fb_width / dpi_scale;
    const float win_height = fb_height / dpi_scale;

    if (!state.ready) {
        return;
    }

    state.time += dt;

    sg_begin_pass(&(sg_pass){
        .action = pass_action,
        .swapchain = sglue_swapchain(),
    });

    nvgBeginFrame(state.vg, win_width, win_height, dpi_scale);
    renderDemo(state.vg, state.mouse_x, state.mouse_y, win_width, win_height, (float)state.time, state.blowup, &state.demo);
    renderGraph(state.vg, 5.0f, 5.0f, &state.fps);
    renderGraph(state.vg, 210.0f, 5.0f, &state.cpu);
    nvgEndFrame(state.vg);

    sg_end_pass();
    sg_commit();

    updateGraph(&state.fps, (float)dt);
    updateGraph(&state.cpu, (float)(sapp_frame_duration()));
}

static void cleanup(void) {
    if (state.vg != NULL) {
        freeDemoData(state.vg, &state.demo);
        nvgDeleteSokol(state.vg);
        state.vg = NULL;
    }
    sg_shutdown();
}

static void event(const sapp_event* ev) {
    const float dpi_scale = sapp_dpi_scale() > 0.0f ? sapp_dpi_scale() : 1.0f;

    switch (ev->type) {
    case SAPP_EVENTTYPE_MOUSE_MOVE:
    case SAPP_EVENTTYPE_MOUSE_DOWN:
    case SAPP_EVENTTYPE_MOUSE_UP:
    case SAPP_EVENTTYPE_MOUSE_ENTER:
        state.mouse_x = ev->mouse_x / dpi_scale;
        state.mouse_y = ev->mouse_y / dpi_scale;
        break;
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (ev->key_repeat) {
            break;
        }
        switch (ev->key_code) {
        case SAPP_KEYCODE_ESCAPE:
            sapp_request_quit();
            break;
        case SAPP_KEYCODE_SPACE:
            state.blowup = !state.blowup;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .width = 1000,
        .height = 600,
        .sample_count = 1,
        .high_dpi = true,
        .window_title = "NanoVG",
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .logger.func = slog_func,
    };
}
