#ifndef GLFW_GLUE_H
#define GLFW_GLUE_H

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width;
    int height;
    int sample_count;
    int swap_interval;
    const char* title;
} glfw_desc_t;

void glfw_init(const glfw_desc_t* desc);
void glfw_shutdown(void);
GLFWwindow* glfw_window(void);
int glfw_width(void);
int glfw_height(void);
float glfw_dpi_scale(void);
sg_environment glfw_environment(void);
sg_swapchain glfw_swapchain(void);

#ifdef __cplusplus
}
#endif

// Implementation
#ifdef GLFW_GLUE_IMPL

#include <assert.h>

static int _glfw_sample_count;
static int _glfw_swap_interval;
static GLFWwindow* _glfw_window;

#define _glfw_def(val, def) (((val) == 0) ? (def) : (val))

void glfw_init(const glfw_desc_t* desc) {
    assert(desc);
    assert(desc->width > 0);
    assert(desc->height > 0);
    assert(desc->title);

    _glfw_sample_count = _glfw_def(desc->sample_count, 1);
    _glfw_swap_interval = desc->swap_interval;

    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, (_glfw_sample_count == 1) ? 0 : _glfw_sample_count);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    _glfw_window = glfwCreateWindow(desc->width, desc->height, desc->title, NULL, NULL);
    glfwMakeContextCurrent(_glfw_window);
    glfwSwapInterval(_glfw_swap_interval);
}

void glfw_shutdown(void) {
    glfwDestroyWindow(_glfw_window);
    glfwTerminate();
}

GLFWwindow* glfw_window(void) {
    return _glfw_window;
}

int glfw_width(void) {
    int width, height;
    glfwGetFramebufferSize(_glfw_window, &width, &height);
    return width;
}

int glfw_height(void) {
    int width, height;
    glfwGetFramebufferSize(_glfw_window, &width, &height);
    return height;
}

float glfw_dpi_scale(void) {
    float xscale, yscale;
    glfwGetWindowContentScale(_glfw_window, &xscale, &yscale);
    return xscale > yscale ? xscale : yscale;
}

sg_environment glfw_environment(void) {
    return (sg_environment) {
        .defaults = {
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
            .sample_count = _glfw_sample_count,
        },
    };
}

sg_swapchain glfw_swapchain(void) {
    int width, height;
    glfwGetFramebufferSize(_glfw_window, &width, &height);
    return (sg_swapchain) {
        .width = width,
        .height = height,
        .sample_count = _glfw_sample_count,
        .color_format = SG_PIXELFORMAT_RGBA8,
        .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
        .gl = {
            .framebuffer = 0,
        }
    };
}

#endif // GLFW_GLUE_IMPL

#endif // GLFW_GLUE_H
