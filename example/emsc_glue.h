#ifndef EMSC_GLUE_H
#define EMSC_GLUE_H

/*
    Emscripten WebGL glue layer for sokol-gfx integration.
    Based on sokol-samples emsc.h
*/

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* canvas;
    int sample_count;
} emsc_desc_t;

void emsc_init(const emsc_desc_t* desc);
int emsc_width(void);
int emsc_height(void);
float emsc_dpi_scale(void);
sg_environment emsc_environment(void);
sg_swapchain emsc_swapchain(void);

#ifdef __cplusplus
}
#endif

// Implementation
#ifdef EMSC_GLUE_IMPL

static const char* _emsc_canvas;
static int _emsc_sample_count;
static double _emsc_width;
static double _emsc_height;
static GLint _emsc_framebuffer;

static EM_BOOL _emsc_size_changed(int event_type, const EmscriptenUiEvent* ui_event, void* user_data) {
    (void)event_type;
    (void)ui_event;
    (void)user_data;
    emscripten_get_element_css_size(_emsc_canvas, &_emsc_width, &_emsc_height);
    emscripten_set_canvas_element_size(_emsc_canvas, (int)_emsc_width, (int)_emsc_height);
    return EM_TRUE;
}

void emsc_init(const emsc_desc_t* desc) {
    _emsc_canvas = desc->canvas ? desc->canvas : "#canvas";
    _emsc_sample_count = desc->sample_count > 0 ? desc->sample_count : 1;

    emscripten_get_element_css_size(_emsc_canvas, &_emsc_width, &_emsc_height);
    emscripten_set_canvas_element_size(_emsc_canvas, (int)_emsc_width, (int)_emsc_height);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, EM_FALSE, _emsc_size_changed);

    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.antialias = (_emsc_sample_count > 1) ? EM_TRUE : EM_FALSE;
    attrs.majorVersion = 2;
    attrs.stencil = EM_TRUE;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(_emsc_canvas, &attrs);
    emscripten_webgl_make_context_current(ctx);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_emsc_framebuffer);
}

int emsc_width(void) {
    return (int)_emsc_width;
}

int emsc_height(void) {
    return (int)_emsc_height;
}

float emsc_dpi_scale(void) {
    return 1.0f;
}

sg_environment emsc_environment(void) {
    return (sg_environment){
        .defaults = {
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
            .sample_count = _emsc_sample_count,
        },
    };
}

sg_swapchain emsc_swapchain(void) {
    return (sg_swapchain){
        .width = (int)_emsc_width,
        .height = (int)_emsc_height,
        .sample_count = _emsc_sample_count,
        .color_format = SG_PIXELFORMAT_RGBA8,
        .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
        .gl = {
            .framebuffer = (uint32_t)_emsc_framebuffer,
        },
    };
}

#endif // EMSC_GLUE_IMPL

#endif // EMSC_GLUE_H
