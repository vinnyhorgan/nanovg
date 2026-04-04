#include <stdbool.h>
#include <stdio.h>

#define SOKOL_IMPL
#include "sokol_gfx.h"
#include "sokol_log.h"

#define GLFW_GLUE_IMPL
#include "glfw_glue.h"

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
    double prev_time;
    float mouse_x;
    float mouse_y;
    bool blowup;
} app_state_t;

static app_state_t state;

static const char* backend_name(sg_backend backend) {
    switch (backend) {
    case SG_BACKEND_GLCORE:
        return "OpenGL Core";
    case SG_BACKEND_GLES3:
        return "OpenGL ES 3";
    case SG_BACKEND_D3D11:
        return "D3D11";
    case SG_BACKEND_METAL_IOS:
        return "Metal iOS";
    case SG_BACKEND_METAL_MACOS:
        return "Metal macOS";
    case SG_BACKEND_METAL_SIMULATOR:
        return "Metal Simulator";
    case SG_BACKEND_WGPU:
        return "WebGPU";
    case SG_BACKEND_VULKAN:
        return "Vulkan";
    case SG_BACKEND_DUMMY:
        return "Dummy";
    default:
        return "Unknown";
    }
}

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

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    if (action != GLFW_PRESS) {
        return;
    }
    switch (key) {
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;
    case GLFW_KEY_SPACE:
        state.blowup = !state.blowup;
        break;
    default:
        break;
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    state.mouse_x = (float)xpos;
    state.mouse_y = (float)ypos;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Initialize GLFW and create window
    glfw_init(&(glfw_desc_t){
        .width = 1000,
        .height = 600,
        .sample_count = 4,
        .swap_interval = 0,
        .title = "NanoVG",
    });

    GLFWwindow* window = glfw_window();
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    // Setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });

    // Update window title with backend name
    char window_title[64];
    snprintf(window_title, sizeof(window_title), "NanoVG (%s)", backend_name(sg_query_backend()));
    glfwSetWindowTitle(window, window_title);

    // Initialize perf graphs
    initGraph(&state.fps, GRAPH_RENDER_FPS, "Frame Time");
    initGraph(&state.cpu, GRAPH_RENDER_MS, "CPU Time");

    // Create NanoVG context
    state.vg = nvgCreateSokol(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
    if (state.vg == NULL) {
        fprintf(stderr, "Could not init nanovg.\n");
        return -1;
    }

    // Load demo data
    if (loadDemoData(state.vg, &state.demo) == -1) {
        fprintf(stderr, "Could not load demo assets.\n");
        return -1;
    }

    state.prev_time = glfwGetTime();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        double curr_time = glfwGetTime();
        double dt = curr_time - state.prev_time;
        state.prev_time = curr_time;

        int fb_width = glfw_width();
        int fb_height = glfw_height();
        float dpi_scale = glfw_dpi_scale();
        float win_width = (float)fb_width / dpi_scale;
        float win_height = (float)fb_height / dpi_scale;

        sg_begin_pass(&(sg_pass){
            .action = pass_action,
            .swapchain = glfw_swapchain(),
        });

        nvgBeginFrame(state.vg, win_width, win_height, dpi_scale);
        renderDemo(state.vg, state.mouse_x, state.mouse_y, win_width, win_height, (float)curr_time, state.blowup, &state.demo);
        renderGraph(state.vg, 5.0f, 5.0f, &state.fps);
        renderGraph(state.vg, 210.0f, 5.0f, &state.cpu);
        nvgEndFrame(state.vg);

        sg_end_pass();
        sg_commit();

        glfwSwapBuffers(window);
        glfwPollEvents();

        updateGraph(&state.fps, (float)dt);
        updateGraph(&state.cpu, (float)dt);
    }

    // Cleanup
    freeDemoData(state.vg, &state.demo);
    nvgDeleteSokol(state.vg);
    sg_shutdown();
    glfw_shutdown();

    return 0;
}
