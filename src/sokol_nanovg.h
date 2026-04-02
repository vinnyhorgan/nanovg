#if defined(SOKOL_IMPL) && !defined(SOKOL_NANOVG_IMPL)
#define SOKOL_NANOVG_IMPL
#endif
#ifndef SOKOL_NANOVG_INCLUDED
/*
    sokol_nanovg.h -- NanoVG rendering backend for sokol_gfx.h

    Project URL: https://github.com/aspect/sokol_nanovg

    Do this:
        #define SOKOL_NANOVG_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    Optionally provide the following defines with your own implementations:
        SOKOL_NANOVG_API_DECL    - public function declaration prefix (default: extern)
        SOKOL_API_IMPL           - public function implementation prefix (default: -)
        SOKOL_ASSERT(c)          - your own assert macro (default: assert(c))

    Include the following headers before including sokol_nanovg.h:
        sokol_gfx.h

    Include the following headers before including the implementation:
        nanovg.h

    FEATURE OVERVIEW
    ================
    sokol_nanovg.h is a NanoVG rendering backend that uses sokol_gfx.h for
    cross-platform GPU rendering. It supports:

    - All sokol_gfx backends: OpenGL, OpenGL ES, D3D11, Metal, WebGPU
    - Gradient fills (linear, radial, box)
    - Image fills with tinting
    - Stroke rendering with anti-aliasing
    - Complex polygon fills using stencil buffer
    - Text rendering (via NanoVG's built-in fontstash integration)

    QUICK START
    ===========
    1. Create sokol_gfx context (sg_setup)
    2. Create NanoVG context with sokol backend:

        NVGcontext* vg = nvgCreateSokol(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

    3. In your render loop:

        int width, height;
        // get window dimensions...

        nvgBeginFrame(vg, width, height, devicePixelRatio);
        // draw with NanoVG API...
        nvgEndFrame(vg);

    4. Cleanup:

        nvgDeleteSokol(vg);

    MEMORY ALLOCATION
    =================
    By default, sokol_nanovg.h uses malloc/free. You can override this by
    providing custom allocator callbacks in snvg_desc_t when calling
    nvgCreateSokolWithDesc().

    LICENSE
    =======
    zlib/libpng license

    Copyright (c) 2024

    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.

        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.

        3. This notice may not be removed or altered from any source
        distribution.
*/
#define SOKOL_NANOVG_INCLUDED (1)

#if !defined(SOKOL_GFX_INCLUDED)
#error "Please include sokol_gfx.h before sokol_nanovg.h"
#endif

#if defined(SOKOL_API_DECL) && !defined(SOKOL_NANOVG_API_DECL)
#define SOKOL_NANOVG_API_DECL SOKOL_API_DECL
#endif
#ifndef SOKOL_NANOVG_API_DECL
#if defined(_WIN32) && defined(SOKOL_DLL) && defined(SOKOL_NANOVG_IMPL)
#define SOKOL_NANOVG_API_DECL __declspec(dllexport)
#elif defined(_WIN32) && defined(SOKOL_DLL)
#define SOKOL_NANOVG_API_DECL __declspec(dllimport)
#else
#define SOKOL_NANOVG_API_DECL extern
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* NanoVG creation flags */
enum {
    NVG_ANTIALIAS       = 1<<0, /* Enable anti-aliasing */
    NVG_STENCIL_STROKES = 1<<1, /* Enable stencil strokes (higher quality) */
    NVG_DEBUG           = 1<<2, /* Enable debug checks */
};

/* Forward declarations */
typedef struct NVGcontext NVGcontext;

/* Allocator interface (matches sokol pattern) */
typedef struct snvg_allocator_t {
    void* (*alloc_fn)(size_t size, void* user_data);
    void (*free_fn)(void* ptr, void* user_data);
    void* user_data;
} snvg_allocator_t;

/* Description for creating NanoVG sokol context */
typedef struct snvg_desc_t {
    int max_vertices;       /* Max vertices per frame (default: 8192) */
    int max_commands;       /* Max draw commands per frame (default: 256) */
    snvg_allocator_t allocator; /* Custom allocator (optional) */
} snvg_desc_t;

/* Create NanoVG context with sokol backend */
SOKOL_NANOVG_API_DECL NVGcontext* nvgCreateSokol(int flags);

/* Create NanoVG context with sokol backend and custom configuration */
SOKOL_NANOVG_API_DECL NVGcontext* nvgCreateSokolWithDesc(int flags, const snvg_desc_t* desc);

/* Delete NanoVG context */
SOKOL_NANOVG_API_DECL void nvgDeleteSokol(NVGcontext* ctx);

/* Set current render pass view (for off-screen rendering) */
SOKOL_NANOVG_API_DECL void snvg_set_view(NVGcontext* ctx, sg_image color_img, sg_image depth_img);

/* Reset to default framebuffer rendering */
SOKOL_NANOVG_API_DECL void snvg_reset_view(NVGcontext* ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOKOL_NANOVG_INCLUDED */

/*--- IMPLEMENTATION ---------------------------------------------------------*/
#ifdef SOKOL_NANOVG_IMPL
#define SOKOL_SHDC_IMPL

#ifndef SOKOL_API_IMPL
#define SOKOL_API_IMPL
#endif
#ifndef SOKOL_ASSERT
#include <assert.h>
#define SOKOL_ASSERT(c) assert(c)
#endif
#ifndef SOKOL_UNREACHABLE
#define SOKOL_UNREACHABLE SOKOL_ASSERT(false)
#endif

#include <stdlib.h>
#include <string.h>
#include <math.h>

#if !defined(NANOVG_H)
#error "Please include nanovg.h before the sokol_nanovg.h implementation"
#endif

/*=== SHADER TYPE DEFINITIONS ================================================*/

/* Vec2 type for shader uniforms */
typedef struct snvg_vec2 {
    float x, y;
} snvg_vec2;

/* Vec4 type for shader uniforms */
typedef struct snvg_vec4 {
    float x, y, z, w;
} snvg_vec4;

/* Mat4 type (not actually used but needed by shader header) */
typedef struct snvg_mat4 {
    float m[16];
} snvg_mat4;

/*=== SHADER INCLUDE =========================================================*/
/* Include the generated shader header (created by sokol-shdc) */
#include "snvg_shader.h"

/*=== INTERNAL TYPES =========================================================*/

/* Shader types - matches nanovg_gl.h */
enum SNVGshaderType {
    SNVG_SHADER_FILLGRAD,
    SNVG_SHADER_FILLIMG,
    SNVG_SHADER_SIMPLE,
    SNVG_SHADER_IMG
};

/* Draw call types */
enum SNVGcallType {
    SNVG_NONE = 0,
    SNVG_FILL,
    SNVG_CONVEXFILL,
    SNVG_STROKE,
    SNVG_TRIANGLES,
};

/* Texture entry */
typedef struct SNVGtexture {
    int id;
    sg_image img;
    sg_view tex_view;
    sg_sampler smp;
    int width, height;
    int type;
    int flags;
    int dirty;                      /* Flag indicating texture needs update */
    unsigned char* pending_data;    /* Pending data for deferred update */
} SNVGtexture;

/* Draw call */
typedef struct SNVGcall {
    int type;
    int image;
    int pathOffset;
    int pathCount;
    int triangleOffset;
    int triangleCount;
    int uniformOffset;
    sg_blend_state blendFunc;
} SNVGcall;

/* Path data */
typedef struct SNVGpath {
    int fillOffset;
    int fillCount;
    int strokeOffset;
    int strokeCount;
} SNVGpath;

/* Fragment uniforms - matches shader fs_params */
typedef struct SNVGfragUniforms {
    float scissorMat[12];   /* 3 vec4s */
    float paintMat[12];     /* 3 vec4s */
    float innerCol[4];
    float outerCol[4];
    float scissorExt[2];
    float scissorScale[2];
    float extent[2];
    float radius;
    float feather;
    float strokeMult;
    float strokeThr;
    float texType;
    float type;
} SNVGfragUniforms;

/* Main context */
typedef struct SNVGcontext {
    /* Sokol resources */
    sg_shader shader;
    sg_pipeline pip_fill;           /* For convex fill and triangles */
    sg_pipeline pip_fill_stencil;   /* For stencil fill pass 1 */
    sg_pipeline pip_fill_antialias; /* For stencil fill pass 2 (AA) */
    sg_pipeline pip_fill_draw;      /* For stencil fill pass 3 (draw) */
    sg_pipeline pip_stroke;         /* For stroke rendering */
    sg_pipeline pip_stroke_stencil; /* For stencil stroke pass 1 */
    sg_pipeline pip_stroke_antialias; /* For stencil stroke pass 2 */
    sg_pipeline pip_stroke_clear;   /* For stencil stroke clear */
    sg_pipeline pip_triangles;      /* For textured triangles */
    sg_buffer vbuf;
    sg_sampler default_sampler;
    sg_image dummy_tex;
    sg_view dummy_view;
    sg_bindings bindings;

    /* View state */
    float view[2];

    /* Texture management */
    SNVGtexture* textures;
    int ntextures;
    int ctextures;
    int textureId;

    /* Per-frame buffers */
    SNVGcall* calls;
    int ccalls;
    int ncalls;
    SNVGpath* paths;
    int cpaths;
    int npaths;
    struct NVGvertex* verts;
    int cverts;
    int nverts;
    unsigned char* uniforms;
    int cuniforms;
    int nuniforms;

    /* Flags and settings */
    int flags;
    int fragSize;

    /* Allocator */
    snvg_allocator_t allocator;
} SNVGcontext;

/*=== HELPER FUNCTIONS =======================================================*/

static int snvg__maxi(int a, int b) { return a > b ? a : b; }
/* Convert triangle fan vertex count to triangle list vertex count */
static int snvg__fanToTriCount(int fanCount) {
    if (fanCount < 3) return 0;
    return (fanCount - 2) * 3;
}

/* Convert triangle fan vertices to triangle list vertices
   Returns the number of triangle list vertices written */
static int snvg__fanToTriangles(struct NVGvertex* dst, const struct NVGvertex* src, int fanCount) {
    int i, triCount = 0;
    if (fanCount < 3) return 0;
    /* Fan: v0 is center, then v1, v2, v3...
       Triangles: (v0, v1, v2), (v0, v2, v3), ... */
    for (i = 2; i < fanCount; i++) {
        dst[triCount++] = src[0];      /* Center vertex */
        dst[triCount++] = src[i - 1];  /* Previous vertex */
        dst[triCount++] = src[i];      /* Current vertex */
    }
    return triCount;
}

static void* snvg__alloc(SNVGcontext* ctx, size_t size) {
    if (ctx->allocator.alloc_fn) {
        return ctx->allocator.alloc_fn(size, ctx->allocator.user_data);
    }
    return malloc(size);
}

static void* snvg__realloc(SNVGcontext* ctx, void* ptr, size_t old_size, size_t new_size) {
    if (ctx->allocator.alloc_fn) {
        void* new_ptr = ctx->allocator.alloc_fn(new_size, ctx->allocator.user_data);
        if (new_ptr && ptr) {
            memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
            ctx->allocator.free_fn(ptr, ctx->allocator.user_data);
        }
        return new_ptr;
    }
    return realloc(ptr, new_size);
}

static void snvg__free(SNVGcontext* ctx, void* ptr) {
    if (ctx->allocator.free_fn) {
        ctx->allocator.free_fn(ptr, ctx->allocator.user_data);
    } else {
        free(ptr);
    }
}

/*=== TEXTURE MANAGEMENT =====================================================*/

static SNVGtexture* snvg__allocTexture(SNVGcontext* ctx) {
    SNVGtexture* tex = NULL;
    int i;

    for (i = 0; i < ctx->ntextures; i++) {
        if (ctx->textures[i].id == 0) {
            tex = &ctx->textures[i];
            break;
        }
    }
    if (tex == NULL) {
        if (ctx->ntextures + 1 > ctx->ctextures) {
            int ctextures = snvg__maxi(ctx->ntextures + 1, 4) + ctx->ctextures / 2;
            SNVGtexture* textures = (SNVGtexture*)snvg__realloc(ctx, ctx->textures,
                sizeof(SNVGtexture) * ctx->ctextures, sizeof(SNVGtexture) * ctextures);
            if (textures == NULL) return NULL;
            ctx->textures = textures;
            ctx->ctextures = ctextures;
        }
        tex = &ctx->textures[ctx->ntextures++];
    }

    memset(tex, 0, sizeof(*tex));
    tex->id = ++ctx->textureId;

    return tex;
}

static SNVGtexture* snvg__findTexture(SNVGcontext* ctx, int id) {
    int i;
    for (i = 0; i < ctx->ntextures; i++) {
        if (ctx->textures[i].id == id) {
            return &ctx->textures[i];
        }
    }
    return NULL;
}

static int snvg__deleteTexture(SNVGcontext* ctx, int id) {
    int i;
    for (i = 0; i < ctx->ntextures; i++) {
        if (ctx->textures[i].id == id) {
            if (ctx->textures[i].img.id != SG_INVALID_ID) {
                sg_destroy_image(ctx->textures[i].img);
            }
            if (ctx->textures[i].tex_view.id != SG_INVALID_ID) {
                sg_destroy_view(ctx->textures[i].tex_view);
            }
            if (ctx->textures[i].smp.id != SG_INVALID_ID) {
                sg_destroy_sampler(ctx->textures[i].smp);
            }
            /* Free pending data buffer if allocated */
            if (ctx->textures[i].pending_data != NULL) {
                snvg__free(ctx, ctx->textures[i].pending_data);
            }
            memset(&ctx->textures[i], 0, sizeof(ctx->textures[i]));
            return 1;
        }
    }
    return 0;
}

/*=== BLEND STATE HELPERS ====================================================*/

static sg_blend_state snvg__blendCompositeOperation(NVGcompositeOperationState op) {
    sg_blend_factor src_factor, dst_factor;

    /* Convert NanoVG blend factors to sokol blend factors */
    switch (op.srcRGB) {
        case NVG_ZERO:                src_factor = SG_BLENDFACTOR_ZERO; break;
        case NVG_ONE:                 src_factor = SG_BLENDFACTOR_ONE; break;
        case NVG_SRC_COLOR:           src_factor = SG_BLENDFACTOR_SRC_COLOR; break;
        case NVG_ONE_MINUS_SRC_COLOR: src_factor = SG_BLENDFACTOR_ONE_MINUS_SRC_COLOR; break;
        case NVG_DST_COLOR:           src_factor = SG_BLENDFACTOR_DST_COLOR; break;
        case NVG_ONE_MINUS_DST_COLOR: src_factor = SG_BLENDFACTOR_ONE_MINUS_DST_COLOR; break;
        case NVG_SRC_ALPHA:           src_factor = SG_BLENDFACTOR_SRC_ALPHA; break;
        case NVG_ONE_MINUS_SRC_ALPHA: src_factor = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA; break;
        case NVG_DST_ALPHA:           src_factor = SG_BLENDFACTOR_DST_ALPHA; break;
        case NVG_ONE_MINUS_DST_ALPHA: src_factor = SG_BLENDFACTOR_ONE_MINUS_DST_ALPHA; break;
        case NVG_SRC_ALPHA_SATURATE:  src_factor = SG_BLENDFACTOR_SRC_ALPHA_SATURATED; break;
        default:                      src_factor = SG_BLENDFACTOR_ONE; break;
    }

    switch (op.dstRGB) {
        case NVG_ZERO:                dst_factor = SG_BLENDFACTOR_ZERO; break;
        case NVG_ONE:                 dst_factor = SG_BLENDFACTOR_ONE; break;
        case NVG_SRC_COLOR:           dst_factor = SG_BLENDFACTOR_SRC_COLOR; break;
        case NVG_ONE_MINUS_SRC_COLOR: dst_factor = SG_BLENDFACTOR_ONE_MINUS_SRC_COLOR; break;
        case NVG_DST_COLOR:           dst_factor = SG_BLENDFACTOR_DST_COLOR; break;
        case NVG_ONE_MINUS_DST_COLOR: dst_factor = SG_BLENDFACTOR_ONE_MINUS_DST_COLOR; break;
        case NVG_SRC_ALPHA:           dst_factor = SG_BLENDFACTOR_SRC_ALPHA; break;
        case NVG_ONE_MINUS_SRC_ALPHA: dst_factor = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA; break;
        case NVG_DST_ALPHA:           dst_factor = SG_BLENDFACTOR_DST_ALPHA; break;
        case NVG_ONE_MINUS_DST_ALPHA: dst_factor = SG_BLENDFACTOR_ONE_MINUS_DST_ALPHA; break;
        case NVG_SRC_ALPHA_SATURATE:  dst_factor = SG_BLENDFACTOR_SRC_ALPHA_SATURATED; break;
        default:                      dst_factor = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA; break;
    }

    sg_blend_state blend = {
        .enabled = true,
        .src_factor_rgb = src_factor,
        .dst_factor_rgb = dst_factor,
        .src_factor_alpha = src_factor,
        .dst_factor_alpha = dst_factor,
    };
    return blend;
}

/*=== UNIFORM CONVERSION =====================================================*/

static void snvg__xformToMat3x4(float* m3, float* t) {
    /* Convert 2D transform (6 floats) to mat3 stored as 3 vec4s */
    m3[0] = t[0];
    m3[1] = t[1];
    m3[2] = 0.0f;
    m3[3] = 0.0f;  /* padding */
    m3[4] = t[2];
    m3[5] = t[3];
    m3[6] = 0.0f;
    m3[7] = 0.0f;  /* padding */
    m3[8] = t[4];
    m3[9] = t[5];
    m3[10] = 1.0f;
    m3[11] = 0.0f; /* padding */
}

static NVGcolor snvg__premulColor(NVGcolor c) {
    c.r *= c.a;
    c.g *= c.a;
    c.b *= c.a;
    return c;
}

static int snvg__convertPaint(SNVGcontext* ctx, SNVGfragUniforms* frag, NVGpaint* paint,
                              NVGscissor* scissor, float width, float fringe, float strokeThr) {
    SNVGtexture* tex = NULL;
    float invxform[6];
    NVGcolor innerCol, outerCol;

    memset(frag, 0, sizeof(*frag));

    innerCol = snvg__premulColor(paint->innerColor);
    outerCol = snvg__premulColor(paint->outerColor);
    frag->innerCol[0] = innerCol.r;
    frag->innerCol[1] = innerCol.g;
    frag->innerCol[2] = innerCol.b;
    frag->innerCol[3] = innerCol.a;
    frag->outerCol[0] = outerCol.r;
    frag->outerCol[1] = outerCol.g;
    frag->outerCol[2] = outerCol.b;
    frag->outerCol[3] = outerCol.a;

    if (scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f) {
        memset(frag->scissorMat, 0, sizeof(frag->scissorMat));
        frag->scissorExt[0] = 1.0f;
        frag->scissorExt[1] = 1.0f;
        frag->scissorScale[0] = 1.0f;
        frag->scissorScale[1] = 1.0f;
    } else {
        nvgTransformInverse(invxform, scissor->xform);
        snvg__xformToMat3x4(frag->scissorMat, invxform);
        frag->scissorExt[0] = scissor->extent[0];
        frag->scissorExt[1] = scissor->extent[1];
        frag->scissorScale[0] = sqrtf(scissor->xform[0]*scissor->xform[0] + scissor->xform[2]*scissor->xform[2]) / fringe;
        frag->scissorScale[1] = sqrtf(scissor->xform[1]*scissor->xform[1] + scissor->xform[3]*scissor->xform[3]) / fringe;
    }

    frag->extent[0] = paint->extent[0];
    frag->extent[1] = paint->extent[1];
    frag->strokeMult = (width * 0.5f + fringe * 0.5f) / fringe;
    frag->strokeThr = strokeThr;

    if (paint->image != 0) {
        tex = snvg__findTexture(ctx, paint->image);
        if (tex == NULL) return 0;
        if ((tex->flags & NVG_IMAGE_FLIPY) != 0) {
            float m1[6], m2[6];
            nvgTransformTranslate(m1, 0.0f, frag->extent[1] * 0.5f);
            nvgTransformMultiply(m1, paint->xform);
            nvgTransformScale(m2, 1.0f, -1.0f);
            nvgTransformMultiply(m2, m1);
            nvgTransformTranslate(m1, 0.0f, -frag->extent[1] * 0.5f);
            nvgTransformMultiply(m1, m2);
            nvgTransformInverse(invxform, m1);
        } else {
            nvgTransformInverse(invxform, paint->xform);
        }
        frag->type = (float)SNVG_SHADER_FILLIMG;

        if (tex->type == NVG_TEXTURE_RGBA)
            frag->texType = (tex->flags & NVG_IMAGE_PREMULTIPLIED) ? 0.0f : 1.0f;
        else
            frag->texType = 2.0f;
    } else {
        frag->type = (float)SNVG_SHADER_FILLGRAD;
        frag->radius = paint->radius;
        frag->feather = paint->feather;
        nvgTransformInverse(invxform, paint->xform);
    }

    snvg__xformToMat3x4(frag->paintMat, invxform);

    return 1;
}

/*=== UNIFORM BUFFER MANAGEMENT ==============================================*/

static int snvg__allocFragUniforms(SNVGcontext* ctx, int n) {
    int ret = 0, structSize = sizeof(SNVGfragUniforms);
    if (ctx->nuniforms + n > ctx->cuniforms) {
        unsigned char* uniforms;
        int cuniforms = snvg__maxi(ctx->nuniforms + n, 128) + ctx->cuniforms / 2;
        uniforms = (unsigned char*)snvg__realloc(ctx, ctx->uniforms,
            structSize * ctx->cuniforms, structSize * cuniforms);
        if (uniforms == NULL) return -1;
        ctx->uniforms = uniforms;
        ctx->cuniforms = cuniforms;
    }
    ret = ctx->nuniforms * structSize;
    ctx->nuniforms += n;
    return ret;
}

static SNVGfragUniforms* snvg__fragUniformPtr(SNVGcontext* ctx, int i) {
    return (SNVGfragUniforms*)&ctx->uniforms[i];
}

/*=== DRAW CALL MANAGEMENT ===================================================*/

static SNVGcall* snvg__allocCall(SNVGcontext* ctx) {
    SNVGcall* ret = NULL;
    if (ctx->ncalls + 1 > ctx->ccalls) {
        SNVGcall* calls;
        int ccalls = snvg__maxi(ctx->ncalls + 1, 128) + ctx->ccalls / 2;
        calls = (SNVGcall*)snvg__realloc(ctx, ctx->calls,
            sizeof(SNVGcall) * ctx->ccalls, sizeof(SNVGcall) * ccalls);
        if (calls == NULL) return NULL;
        ctx->calls = calls;
        ctx->ccalls = ccalls;
    }
    ret = &ctx->calls[ctx->ncalls++];
    memset(ret, 0, sizeof(*ret));
    return ret;
}

static int snvg__allocPaths(SNVGcontext* ctx, int n) {
    int ret = 0;
    if (ctx->npaths + n > ctx->cpaths) {
        SNVGpath* paths;
        int cpaths = snvg__maxi(ctx->npaths + n, 128) + ctx->cpaths / 2;
        paths = (SNVGpath*)snvg__realloc(ctx, ctx->paths,
            sizeof(SNVGpath) * ctx->cpaths, sizeof(SNVGpath) * cpaths);
        if (paths == NULL) return -1;
        ctx->paths = paths;
        ctx->cpaths = cpaths;
    }
    ret = ctx->npaths;
    ctx->npaths += n;
    return ret;
}

static int snvg__allocVerts(SNVGcontext* ctx, int n) {
    int ret = 0;
    if (ctx->nverts + n > ctx->cverts) {
        struct NVGvertex* verts;
        int cverts = snvg__maxi(ctx->nverts + n, 4096) + ctx->cverts / 2;
        verts = (struct NVGvertex*)snvg__realloc(ctx, ctx->verts,
            sizeof(struct NVGvertex) * ctx->cverts, sizeof(struct NVGvertex) * cverts);
        if (verts == NULL) return -1;
        ctx->verts = verts;
        ctx->cverts = cverts;
    }
    ret = ctx->nverts;
    ctx->nverts += n;
    return ret;
}

/*=== RENDER CALLBACK IMPLEMENTATIONS ========================================*/

static int snvg__renderCreate(void* uptr) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;

    /* Create shader */
    ctx->shader = sg_make_shader(snvg_shader_desc(sg_query_backend()));
    if (ctx->shader.id == SG_INVALID_ID) {
        return 0;
    }

    /* Create pipelines */
    /* Base pipeline desc for reuse */
    sg_pipeline_desc pip_desc = {
        .shader = ctx->shader,
        .layout = {
            .attrs = {
                [ATTR_snvg_vertex] = { .format = SG_VERTEXFORMAT_FLOAT2 },
                [ATTR_snvg_tcoord] = { .format = SG_VERTEXFORMAT_FLOAT2 },
            },
        },
        .colors[0] = {
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_ONE,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
            .write_mask = SG_COLORMASK_RGBA,
        },
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL,
        },
        .stencil = {
            .enabled = false,
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .cull_mode = SG_CULLMODE_BACK,
        .face_winding = SG_FACEWINDING_CCW,
        .label = "snvg-pip-fill",
    };

    /* Pipeline for convex fills using triangle-list converted fan geometry */
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    ctx->pip_fill = sg_make_pipeline(&pip_desc);

    /* Pipeline for triangles (same as fill but with different primitive type) */
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pip_desc.label = "snvg-pip-triangles";
    ctx->pip_triangles = sg_make_pipeline(&pip_desc);

    /* Pipeline for stencil fill pass 1 - write to stencil
       Note: sokol doesn't have TRIANGLE_FAN, so we use TRIANGULAR */
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pip_desc.colors[0].write_mask = SG_COLORMASK_NONE;  /* Don't write to color */
    pip_desc.stencil = (sg_stencil_state){
        .enabled = true,
        .front = {
            .compare = SG_COMPAREFUNC_ALWAYS,
            .fail_op = SG_STENCILOP_KEEP,
            .depth_fail_op = SG_STENCILOP_KEEP,
            .pass_op = SG_STENCILOP_INCR_WRAP,
        },
        .back = {
            .compare = SG_COMPAREFUNC_ALWAYS,
            .fail_op = SG_STENCILOP_KEEP,
            .depth_fail_op = SG_STENCILOP_KEEP,
            .pass_op = SG_STENCILOP_DECR_WRAP,
        },
        .read_mask = 0xff,
        .write_mask = 0xff,
        .ref = 0,
    };
    pip_desc.cull_mode = SG_CULLMODE_NONE;
    pip_desc.label = "snvg-pip-fill-stencil";
    ctx->pip_fill_stencil = sg_make_pipeline(&pip_desc);

    /* Pipeline for stencil fill pass 2 - anti-aliased edge */
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
    pip_desc.colors[0].write_mask = SG_COLORMASK_RGBA;
    pip_desc.colors[0].blend = (sg_blend_state){
        .enabled = true,
        .src_factor_rgb = SG_BLENDFACTOR_ONE,
        .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .src_factor_alpha = SG_BLENDFACTOR_ONE,
        .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    };
    pip_desc.stencil = (sg_stencil_state){
        .enabled = true,
        .front = {
            .compare = SG_COMPAREFUNC_EQUAL,
            .fail_op = SG_STENCILOP_KEEP,
            .depth_fail_op = SG_STENCILOP_KEEP,
            .pass_op = SG_STENCILOP_KEEP,
        },
        .back = {
            .compare = SG_COMPAREFUNC_EQUAL,
            .fail_op = SG_STENCILOP_KEEP,
            .depth_fail_op = SG_STENCILOP_KEEP,
            .pass_op = SG_STENCILOP_KEEP,
        },
        .read_mask = 0xff,
        .write_mask = 0xff,
        .ref = 0,
    };
    pip_desc.cull_mode = SG_CULLMODE_BACK;
    pip_desc.label = "snvg-pip-fill-antialias";
    ctx->pip_fill_antialias = sg_make_pipeline(&pip_desc);

    /* Pipeline for stencil fill pass 3 - draw fill with stencil test and clear */
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
    pip_desc.stencil = (sg_stencil_state){
        .enabled = true,
        .front = {
            .compare = SG_COMPAREFUNC_NOT_EQUAL,
            .fail_op = SG_STENCILOP_ZERO,
            .depth_fail_op = SG_STENCILOP_ZERO,
            .pass_op = SG_STENCILOP_ZERO,
        },
        .back = {
            .compare = SG_COMPAREFUNC_NOT_EQUAL,
            .fail_op = SG_STENCILOP_ZERO,
            .depth_fail_op = SG_STENCILOP_ZERO,
            .pass_op = SG_STENCILOP_ZERO,
        },
        .read_mask = 0xff,
        .write_mask = 0xff,
        .ref = 0,
    };
    pip_desc.label = "snvg-pip-fill-draw";
    ctx->pip_fill_draw = sg_make_pipeline(&pip_desc);

    /* Pipeline for stroke rendering */
    pip_desc.stencil.enabled = false;
    pip_desc.label = "snvg-pip-stroke";
    ctx->pip_stroke = sg_make_pipeline(&pip_desc);

    /* Pipeline for stencil stroke pass 1 */
    pip_desc.stencil = (sg_stencil_state){
        .enabled = true,
        .front = {
            .compare = SG_COMPAREFUNC_EQUAL,
            .fail_op = SG_STENCILOP_KEEP,
            .depth_fail_op = SG_STENCILOP_KEEP,
            .pass_op = SG_STENCILOP_INCR_CLAMP,
        },
        .back = {
            .compare = SG_COMPAREFUNC_EQUAL,
            .fail_op = SG_STENCILOP_KEEP,
            .depth_fail_op = SG_STENCILOP_KEEP,
            .pass_op = SG_STENCILOP_INCR_CLAMP,
        },
        .read_mask = 0xff,
        .write_mask = 0xff,
        .ref = 0,
    };
    pip_desc.label = "snvg-pip-stroke-stencil";
    ctx->pip_stroke_stencil = sg_make_pipeline(&pip_desc);

    /* Pipeline for stencil stroke pass 2 (AA) */
    pip_desc.stencil.front.compare = SG_COMPAREFUNC_EQUAL;
    pip_desc.stencil.back.compare = SG_COMPAREFUNC_EQUAL;
    pip_desc.stencil.front.pass_op = SG_STENCILOP_KEEP;
    pip_desc.stencil.back.pass_op = SG_STENCILOP_KEEP;
    pip_desc.label = "snvg-pip-stroke-antialias";
    ctx->pip_stroke_antialias = sg_make_pipeline(&pip_desc);

    /* Pipeline for stencil stroke clear */
    pip_desc.colors[0].write_mask = SG_COLORMASK_NONE;
    pip_desc.stencil = (sg_stencil_state){
        .enabled = true,
        .front = {
            .compare = SG_COMPAREFUNC_ALWAYS,
            .fail_op = SG_STENCILOP_ZERO,
            .depth_fail_op = SG_STENCILOP_ZERO,
            .pass_op = SG_STENCILOP_ZERO,
        },
        .back = {
            .compare = SG_COMPAREFUNC_ALWAYS,
            .fail_op = SG_STENCILOP_ZERO,
            .depth_fail_op = SG_STENCILOP_ZERO,
            .pass_op = SG_STENCILOP_ZERO,
        },
        .read_mask = 0xff,
        .write_mask = 0xff,
        .ref = 0,
    };
    pip_desc.label = "snvg-pip-stroke-clear";
    ctx->pip_stroke_clear = sg_make_pipeline(&pip_desc);

    /* Create vertex buffer (dynamic) */
    sg_buffer_desc vbuf_desc = {
        .usage = {
            .vertex_buffer = true,
            .stream_update = true,
            .immutable = false,
        },
        .size = 65536 * sizeof(struct NVGvertex),
        .label = "snvg-vbuf",
    };
    ctx->vbuf = sg_make_buffer(&vbuf_desc);

    /* Create default sampler */
    sg_sampler_desc smp_desc = {
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .mipmap_filter = SG_FILTER_NEAREST,
        .min_lod = 0.0f,
        .max_lod = 0.0f,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "snvg-sampler",
    };
    ctx->default_sampler = sg_make_sampler(&smp_desc);

    /* Create dummy texture (white 1x1) */
    uint32_t white = 0xFFFFFFFF;
    sg_image_desc dummy_desc = {
        .width = 1,
        .height = 1,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.mip_levels[0] = {
            .ptr = &white,
            .size = sizeof(white),
        },
        .label = "snvg-dummy",
    };
    ctx->dummy_tex = sg_make_image(&dummy_desc);

    /* Create view for dummy texture */
    sg_view_desc dummy_view_desc = {
        .texture.image = ctx->dummy_tex,
        .label = "snvg-dummy-view",
    };
    ctx->dummy_view = sg_make_view(&dummy_view_desc);

    /* Initialize bindings */
    ctx->bindings.vertex_buffers[0] = ctx->vbuf;

    return 1;
}

static int snvg__renderCreateTexture(void* uptr, int type, int w, int h, int imageFlags, const unsigned char* data) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    SNVGtexture* tex = snvg__allocTexture(ctx);
    if (tex == NULL) return 0;

    sg_pixel_format fmt;
    if (type == NVG_TEXTURE_RGBA) {
        fmt = SG_PIXELFORMAT_RGBA8;
    } else {
        fmt = SG_PIXELFORMAT_R8;
    }

    sg_image_desc img_desc = {
        .width = w,
        .height = h,
        .pixel_format = fmt,
        .label = "snvg-texture",
    };

    if (data != NULL) {
        img_desc.usage.immutable = true;
        size_t pitch = (type == NVG_TEXTURE_RGBA) ? w * 4 : w;
        img_desc.data.mip_levels[0] = (sg_range){ .ptr = data, .size = h * pitch };
    } else {
        /* Use stream_update to allow multiple updates per frame (needed for fonts) */
        img_desc.usage.immutable = false;
        img_desc.usage.stream_update = true;
    }

    if (imageFlags & NVG_IMAGE_GENERATE_MIPMAPS) {
        img_desc.num_mipmaps = 0;  /* Auto-generate */
    }

    tex->img = sg_make_image(&img_desc);

    /* Create view for this texture */
    sg_view_desc view_desc = {
        .texture.image = tex->img,
        .label = "snvg-texture-view",
    };
    tex->tex_view = sg_make_view(&view_desc);

    /* Create sampler for this texture */
    sg_filter min_filter = SG_FILTER_LINEAR;
    sg_filter mag_filter = SG_FILTER_LINEAR;
    sg_filter mip_filter = (imageFlags & NVG_IMAGE_GENERATE_MIPMAPS) ? SG_FILTER_LINEAR : SG_FILTER_NEAREST;
    sg_wrap wrap_u = (imageFlags & NVG_IMAGE_REPEATX) ? SG_WRAP_REPEAT : SG_WRAP_CLAMP_TO_EDGE;
    sg_wrap wrap_v = (imageFlags & NVG_IMAGE_REPEATY) ? SG_WRAP_REPEAT : SG_WRAP_CLAMP_TO_EDGE;

    if (imageFlags & NVG_IMAGE_NEAREST) {
        min_filter = SG_FILTER_NEAREST;
        mag_filter = SG_FILTER_NEAREST;
    }

    sg_sampler_desc smp_desc = {
        .min_filter = min_filter,
        .mag_filter = mag_filter,
        .mipmap_filter = mip_filter,
        .min_lod = 0.0f,
        .max_lod = (imageFlags & NVG_IMAGE_GENERATE_MIPMAPS) ? 1000.0f : 0.0f,
        .wrap_u = wrap_u,
        .wrap_v = wrap_v,
    };
    tex->smp = sg_make_sampler(&smp_desc);

    tex->width = w;
    tex->height = h;
    tex->type = type;
    tex->flags = imageFlags;

    return tex->id;
}

static int snvg__renderDeleteTexture(void* uptr, int image) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    return snvg__deleteTexture(ctx, image);
}

static int snvg__renderUpdateTexture(void* uptr, int image, int x, int y, int w, int h, const unsigned char* data) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    SNVGtexture* tex = snvg__findTexture(ctx, image);
    if (tex == NULL) return 0;

    (void)x; (void)y; (void)w; (void)h;  /* Partial update not well-supported */

    /* Defer the texture update - store data and mark dirty */
    /* NanoVG may call updateTexture multiple times per frame for font atlas */
    size_t bpp = (tex->type == NVG_TEXTURE_RGBA) ? 4 : 1;
    size_t data_size = (size_t)(tex->width * tex->height) * bpp;
    
    /* Allocate or reuse pending data buffer */
    if (tex->pending_data == NULL) {
        tex->pending_data = (unsigned char*)snvg__alloc(ctx, data_size);
    }
    if (tex->pending_data != NULL) {
        memcpy(tex->pending_data, data, data_size);
        tex->dirty = 1;
    }

    return 1;
}

/* Flush any pending texture updates - call once per frame before rendering */
static void snvg__flushTextureUpdates(SNVGcontext* ctx) {
    for (int i = 0; i < ctx->ntextures; i++) {
        SNVGtexture* tex = &ctx->textures[i];
        if (tex->dirty && tex->pending_data != NULL) {
            size_t bpp = (tex->type == NVG_TEXTURE_RGBA) ? 4 : 1;
            size_t pitch = (size_t)tex->width * bpp;
            
            sg_image_data img_data = {0};
            img_data.mip_levels[0] = (sg_range){ .ptr = tex->pending_data, .size = (size_t)tex->height * pitch };
            sg_update_image(tex->img, &img_data);
            
            tex->dirty = 0;
        }
    }
}

static int snvg__renderGetTextureSize(void* uptr, int image, int* w, int* h) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    SNVGtexture* tex = snvg__findTexture(ctx, image);
    if (tex == NULL) return 0;
    *w = tex->width;
    *h = tex->height;
    return 1;
}

static void snvg__renderViewport(void* uptr, float width, float height, float devicePixelRatio) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    (void)devicePixelRatio;
    ctx->view[0] = width;
    ctx->view[1] = height;
}

static void snvg__renderCancel(void* uptr) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    ctx->nverts = 0;
    ctx->npaths = 0;
    ctx->ncalls = 0;
    ctx->nuniforms = 0;
}

static void snvg__setUniforms(SNVGcontext* ctx, int uniformOffset, int image) {
    SNVGfragUniforms* frag = snvg__fragUniformPtr(ctx, uniformOffset);

    /* Set vertex shader uniforms (view size) */
    vs_params_t vs_params = {
        .viewSize = { ctx->view[0], ctx->view[1] },
    };
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));

    /* Set fragment uniforms */
    fs_params_t fs_params = {
        .scissorMat0 = { frag->scissorMat[0], frag->scissorMat[1], frag->scissorMat[2], frag->scissorMat[3] },
        .scissorMat1 = { frag->scissorMat[4], frag->scissorMat[5], frag->scissorMat[6], frag->scissorMat[7] },
        .scissorMat2 = { frag->scissorMat[8], frag->scissorMat[9], frag->scissorMat[10], frag->scissorMat[11] },
        .paintMat0 = { frag->paintMat[0], frag->paintMat[1], frag->paintMat[2], frag->paintMat[3] },
        .paintMat1 = { frag->paintMat[4], frag->paintMat[5], frag->paintMat[6], frag->paintMat[7] },
        .paintMat2 = { frag->paintMat[8], frag->paintMat[9], frag->paintMat[10], frag->paintMat[11] },
        .innerCol = { frag->innerCol[0], frag->innerCol[1], frag->innerCol[2], frag->innerCol[3] },
        .outerCol = { frag->outerCol[0], frag->outerCol[1], frag->outerCol[2], frag->outerCol[3] },
        .scissorExtScale = { frag->scissorExt[0], frag->scissorExt[1], frag->scissorScale[0], frag->scissorScale[1] },
        .extentRadiusFeather = { frag->extent[0], frag->extent[1], frag->radius, frag->feather },
        .params = { frag->strokeMult, frag->strokeThr, frag->texType, frag->type },
    };
    sg_apply_uniforms(UB_fs_params, &SG_RANGE(fs_params));

    /* Bind texture */
    if (image != 0) {
        SNVGtexture* tex = snvg__findTexture(ctx, image);
        if (tex != NULL) {
            ctx->bindings.views[VIEW_tex] = tex->tex_view;
            ctx->bindings.samplers[SMP_smp] = tex->smp;
        } else {
            ctx->bindings.views[VIEW_tex] = ctx->dummy_view;
            ctx->bindings.samplers[SMP_smp] = ctx->default_sampler;
        }
    } else {
        ctx->bindings.views[VIEW_tex] = ctx->dummy_view;
        ctx->bindings.samplers[SMP_smp] = ctx->default_sampler;
    }
    sg_apply_bindings(&ctx->bindings);
}

static void snvg__fill(SNVGcontext* ctx, SNVGcall* call) {
    SNVGpath* paths = &ctx->paths[call->pathOffset];
    int i, npaths = call->pathCount;

    /* Draw stencil fill - using odd-even fill rule */
    sg_apply_pipeline(ctx->pip_fill_stencil);
    snvg__setUniforms(ctx, call->uniformOffset, 0);

    for (i = 0; i < npaths; i++) {
        if (paths[i].fillCount > 0) {
            sg_draw(paths[i].fillOffset, paths[i].fillCount, 1);
        }
    }

    /* Draw anti-aliased edges */
    sg_apply_pipeline(ctx->pip_fill_antialias);
    snvg__setUniforms(ctx, call->uniformOffset + ctx->fragSize, call->image);

    if (ctx->flags & NVG_ANTIALIAS) {
        for (i = 0; i < npaths; i++) {
            if (paths[i].strokeCount > 0) {
                sg_draw(paths[i].strokeOffset, paths[i].strokeCount, 1);
            }
        }
    }

    /* Draw fill using stencil */
    sg_apply_pipeline(ctx->pip_fill_draw);
    snvg__setUniforms(ctx, call->uniformOffset + ctx->fragSize, call->image);

    /* Draw a full-screen quad to fill where stencil != 0 */
    sg_draw(call->triangleOffset, call->triangleCount, 1);
}

static void snvg__convexFill(SNVGcontext* ctx, SNVGcall* call) {
    SNVGpath* paths = &ctx->paths[call->pathOffset];
    int i, npaths = call->pathCount;

    sg_apply_pipeline(ctx->pip_fill);
    snvg__setUniforms(ctx, call->uniformOffset, call->image);

    for (i = 0; i < npaths; i++) {
        if (paths[i].fillCount > 0) {
            sg_draw(paths[i].fillOffset, paths[i].fillCount, 1);
        }
        /* Draw fringes (anti-aliased edges) */
        if (paths[i].strokeCount > 0) {
            sg_draw(paths[i].strokeOffset, paths[i].strokeCount, 1);
        }
    }
}

static void snvg__stroke(SNVGcontext* ctx, SNVGcall* call) {
    SNVGpath* paths = &ctx->paths[call->pathOffset];
    int i, npaths = call->pathCount;

    if (ctx->flags & NVG_STENCIL_STROKES) {
        /* Stencil-based stroke for higher quality */
        sg_apply_pipeline(ctx->pip_stroke_stencil);
        snvg__setUniforms(ctx, call->uniformOffset + ctx->fragSize, call->image);

        for (i = 0; i < npaths; i++) {
            if (paths[i].strokeCount > 0) {
                sg_draw(paths[i].strokeOffset, paths[i].strokeCount, 1);
            }
        }

        /* Draw anti-aliased stroke */
        sg_apply_pipeline(ctx->pip_stroke_antialias);
        snvg__setUniforms(ctx, call->uniformOffset, call->image);

        for (i = 0; i < npaths; i++) {
            if (paths[i].strokeCount > 0) {
                sg_draw(paths[i].strokeOffset, paths[i].strokeCount, 1);
            }
        }

        /* Clear stencil */
        sg_apply_pipeline(ctx->pip_stroke_clear);
        snvg__setUniforms(ctx, call->uniformOffset, 0);

        for (i = 0; i < npaths; i++) {
            if (paths[i].strokeCount > 0) {
                sg_draw(paths[i].strokeOffset, paths[i].strokeCount, 1);
            }
        }
    } else {
        /* Simple stroke */
        sg_apply_pipeline(ctx->pip_stroke);
        snvg__setUniforms(ctx, call->uniformOffset, call->image);

        for (i = 0; i < npaths; i++) {
            if (paths[i].strokeCount > 0) {
                sg_draw(paths[i].strokeOffset, paths[i].strokeCount, 1);
            }
        }
    }
}

static void snvg__triangles(SNVGcontext* ctx, SNVGcall* call) {
    sg_apply_pipeline(ctx->pip_triangles);
    snvg__setUniforms(ctx, call->uniformOffset, call->image);
    sg_draw(call->triangleOffset, call->triangleCount, 1);
}

static void snvg__renderFlush(void* uptr) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    int i;

    /* Flush any pending texture updates (deferred from renderUpdateTexture) */
    snvg__flushTextureUpdates(ctx);

    if (ctx->ncalls > 0) {
        /* Upload vertex data */
        sg_range verts_range = { .ptr = ctx->verts, .size = ctx->nverts * sizeof(struct NVGvertex) };
        sg_update_buffer(ctx->vbuf, &verts_range);

        /* Process draw calls */
        for (i = 0; i < ctx->ncalls; i++) {
            SNVGcall* call = &ctx->calls[i];
            switch (call->type) {
                case SNVG_FILL:
                    snvg__fill(ctx, call);
                    break;
                case SNVG_CONVEXFILL:
                    snvg__convexFill(ctx, call);
                    break;
                case SNVG_STROKE:
                    snvg__stroke(ctx, call);
                    break;
                case SNVG_TRIANGLES:
                    snvg__triangles(ctx, call);
                    break;
            }
        }
    }

    /* Reset state for next frame */
    ctx->nverts = 0;
    ctx->npaths = 0;
    ctx->ncalls = 0;
    ctx->nuniforms = 0;
}

static void snvg__renderFill(void* uptr, NVGpaint* paint, NVGcompositeOperationState compositeOperation,
                             NVGscissor* scissor, float fringe, const float* bounds, const NVGpath* paths, int npaths) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    SNVGcall* call = snvg__allocCall(ctx);
    struct NVGvertex* quad;
    SNVGfragUniforms* frag;
    int i, maxverts, offset;

    if (call == NULL) return;

    call->type = SNVG_FILL;
    call->triangleCount = 4;
    call->pathOffset = snvg__allocPaths(ctx, npaths);
    if (call->pathOffset == -1) goto error;
    call->pathCount = npaths;
    call->image = paint->image;
    call->blendFunc = snvg__blendCompositeOperation(compositeOperation);

    if (npaths == 1 && paths[0].convex) {
        call->type = SNVG_CONVEXFILL;
        call->triangleCount = 0;  /* No bounding box quad needed */
    }

    /* Allocate vertices for all fill and stroke verts plus quad */
    maxverts = 0;
    for (i = 0; i < npaths; i++) {
        maxverts += snvg__fanToTriCount(paths[i].nfill);
        maxverts += paths[i].nstroke;
    }
    if (call->type == SNVG_FILL) {
        maxverts += 4;  /* Quad for fill */
    }

    offset = snvg__allocVerts(ctx, maxverts);
    if (offset == -1) goto error;

    for (i = 0; i < npaths; i++) {
        SNVGpath* copy = &ctx->paths[call->pathOffset + i];
        const NVGpath* path = &paths[i];
        memset(copy, 0, sizeof(*copy));
        if (path->nfill > 0) {
            copy->fillOffset = offset;
            copy->fillCount = snvg__fanToTriangles(&ctx->verts[offset], path->fill, path->nfill);
            offset += copy->fillCount;
        }
        if (path->nstroke > 0) {
            copy->strokeOffset = offset;
            copy->strokeCount = path->nstroke;
            memcpy(&ctx->verts[offset], path->stroke, sizeof(struct NVGvertex) * path->nstroke);
            offset += path->nstroke;
        }
    }

    /* Quad for fill */
    if (call->type == SNVG_FILL) {
        call->triangleOffset = offset;
        quad = &ctx->verts[offset];
        /* Simple quad covering the bounds */
        quad[0].x = bounds[2]; quad[0].y = bounds[3]; quad[0].u = 0.5f; quad[0].v = 1.0f;
        quad[1].x = bounds[2]; quad[1].y = bounds[1]; quad[1].u = 0.5f; quad[1].v = 1.0f;
        quad[2].x = bounds[0]; quad[2].y = bounds[3]; quad[2].u = 0.5f; quad[2].v = 1.0f;
        quad[3].x = bounds[0]; quad[3].y = bounds[1]; quad[3].u = 0.5f; quad[3].v = 1.0f;

        call->uniformOffset = snvg__allocFragUniforms(ctx, 2);
        if (call->uniformOffset == -1) goto error;

        /* Simple shader for stencil fill */
        frag = snvg__fragUniformPtr(ctx, call->uniformOffset);
        memset(frag, 0, sizeof(*frag));
        frag->strokeThr = -1.0f;
        frag->type = (float)SNVG_SHADER_SIMPLE;

        /* Fill shader */
        snvg__convertPaint(ctx, snvg__fragUniformPtr(ctx, call->uniformOffset + ctx->fragSize),
                          paint, scissor, fringe, fringe, -1.0f);
    } else {
        call->uniformOffset = snvg__allocFragUniforms(ctx, 1);
        if (call->uniformOffset == -1) goto error;
        snvg__convertPaint(ctx, snvg__fragUniformPtr(ctx, call->uniformOffset),
                          paint, scissor, fringe, fringe, -1.0f);
    }

    return;

error:
    if (ctx->ncalls > 0) ctx->ncalls--;
}

static void snvg__renderStroke(void* uptr, NVGpaint* paint, NVGcompositeOperationState compositeOperation,
                               NVGscissor* scissor, float fringe, float strokeWidth, const NVGpath* paths, int npaths) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    SNVGcall* call = snvg__allocCall(ctx);
    int i, maxverts, offset;

    if (call == NULL) return;

    call->type = SNVG_STROKE;
    call->pathOffset = snvg__allocPaths(ctx, npaths);
    if (call->pathOffset == -1) goto error;
    call->pathCount = npaths;
    call->image = paint->image;
    call->blendFunc = snvg__blendCompositeOperation(compositeOperation);

    /* Allocate vertices */
    maxverts = 0;
    for (i = 0; i < npaths; i++) {
        maxverts += paths[i].nstroke;
    }

    offset = snvg__allocVerts(ctx, maxverts);
    if (offset == -1) goto error;

    for (i = 0; i < npaths; i++) {
        SNVGpath* copy = &ctx->paths[call->pathOffset + i];
        const NVGpath* path = &paths[i];
        memset(copy, 0, sizeof(*copy));
        if (path->nstroke > 0) {
            copy->strokeOffset = offset;
            copy->strokeCount = path->nstroke;
            memcpy(&ctx->verts[offset], path->stroke, sizeof(struct NVGvertex) * path->nstroke);
            offset += path->nstroke;
        }
    }

    if (ctx->flags & NVG_STENCIL_STROKES) {
        call->uniformOffset = snvg__allocFragUniforms(ctx, 2);
        if (call->uniformOffset == -1) goto error;
        snvg__convertPaint(ctx, snvg__fragUniformPtr(ctx, call->uniformOffset),
                          paint, scissor, strokeWidth, fringe, -1.0f);
        snvg__convertPaint(ctx, snvg__fragUniformPtr(ctx, call->uniformOffset + ctx->fragSize),
                          paint, scissor, strokeWidth, fringe, 1.0f - 0.5f/255.0f);
    } else {
        call->uniformOffset = snvg__allocFragUniforms(ctx, 1);
        if (call->uniformOffset == -1) goto error;
        snvg__convertPaint(ctx, snvg__fragUniformPtr(ctx, call->uniformOffset),
                          paint, scissor, strokeWidth, fringe, -1.0f);
    }

    return;

error:
    if (ctx->ncalls > 0) ctx->ncalls--;
}

static void snvg__renderTriangles(void* uptr, NVGpaint* paint, NVGcompositeOperationState compositeOperation,
                                  NVGscissor* scissor, const NVGvertex* verts, int nverts, float fringe) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    SNVGcall* call = snvg__allocCall(ctx);
    SNVGfragUniforms* frag;

    if (call == NULL) return;

    call->type = SNVG_TRIANGLES;
    call->image = paint->image;
    call->blendFunc = snvg__blendCompositeOperation(compositeOperation);

    /* Allocate vertices */
    call->triangleOffset = snvg__allocVerts(ctx, nverts);
    if (call->triangleOffset == -1) goto error;
    call->triangleCount = nverts;

    memcpy(&ctx->verts[call->triangleOffset], verts, sizeof(struct NVGvertex) * nverts);

    /* Allocate uniforms */
    call->uniformOffset = snvg__allocFragUniforms(ctx, 1);
    if (call->uniformOffset == -1) goto error;

    frag = snvg__fragUniformPtr(ctx, call->uniformOffset);
    snvg__convertPaint(ctx, frag, paint, scissor, 1.0f, fringe, -1.0f);
    frag->type = (float)SNVG_SHADER_IMG;

    return;

error:
    if (ctx->ncalls > 0) ctx->ncalls--;
}

static void snvg__renderDelete(void* uptr) {
    SNVGcontext* ctx = (SNVGcontext*)uptr;
    int i;

    if (ctx == NULL) return;

    /* Destroy textures */
    for (i = 0; i < ctx->ntextures; i++) {
        if (ctx->textures[i].img.id != SG_INVALID_ID) {
            sg_destroy_image(ctx->textures[i].img);
        }
        if (ctx->textures[i].tex_view.id != SG_INVALID_ID) {
            sg_destroy_view(ctx->textures[i].tex_view);
        }
        if (ctx->textures[i].smp.id != SG_INVALID_ID) {
            sg_destroy_sampler(ctx->textures[i].smp);
        }
        /* Free pending data buffer if allocated */
        if (ctx->textures[i].pending_data != NULL) {
            snvg__free(ctx, ctx->textures[i].pending_data);
        }
    }

    /* Destroy sokol resources */
    sg_destroy_buffer(ctx->vbuf);
    sg_destroy_sampler(ctx->default_sampler);
    sg_destroy_image(ctx->dummy_tex);
    sg_destroy_view(ctx->dummy_view);
    sg_destroy_pipeline(ctx->pip_fill);
    sg_destroy_pipeline(ctx->pip_fill_stencil);
    sg_destroy_pipeline(ctx->pip_fill_antialias);
    sg_destroy_pipeline(ctx->pip_fill_draw);
    sg_destroy_pipeline(ctx->pip_stroke);
    sg_destroy_pipeline(ctx->pip_stroke_stencil);
    sg_destroy_pipeline(ctx->pip_stroke_antialias);
    sg_destroy_pipeline(ctx->pip_stroke_clear);
    sg_destroy_pipeline(ctx->pip_triangles);
    sg_destroy_shader(ctx->shader);

    /* Free buffers */
    snvg__free(ctx, ctx->textures);
    snvg__free(ctx, ctx->paths);
    snvg__free(ctx, ctx->verts);
    snvg__free(ctx, ctx->uniforms);
    snvg__free(ctx, ctx->calls);
    snvg__free(ctx, ctx);
}

/*=== PUBLIC API =============================================================*/

SOKOL_NANOVG_API_DECL NVGcontext* nvgCreateSokol(int flags) {
    return nvgCreateSokolWithDesc(flags, NULL);
}

SOKOL_NANOVG_API_DECL NVGcontext* nvgCreateSokolWithDesc(int flags, const snvg_desc_t* desc) {
    NVGparams params;
    NVGcontext* ctx = NULL;
    SNVGcontext* sg = NULL;

    sg = (SNVGcontext*)malloc(sizeof(SNVGcontext));
    if (sg == NULL) goto error;
    memset(sg, 0, sizeof(SNVGcontext));

    sg->flags = flags;
    sg->fragSize = sizeof(SNVGfragUniforms);

    if (desc != NULL) {
        sg->allocator = desc->allocator;
    }

    memset(&params, 0, sizeof(params));
    params.renderCreate = snvg__renderCreate;
    params.renderCreateTexture = snvg__renderCreateTexture;
    params.renderDeleteTexture = snvg__renderDeleteTexture;
    params.renderUpdateTexture = snvg__renderUpdateTexture;
    params.renderGetTextureSize = snvg__renderGetTextureSize;
    params.renderViewport = snvg__renderViewport;
    params.renderCancel = snvg__renderCancel;
    params.renderFlush = snvg__renderFlush;
    params.renderFill = snvg__renderFill;
    params.renderStroke = snvg__renderStroke;
    params.renderTriangles = snvg__renderTriangles;
    params.renderDelete = snvg__renderDelete;
    params.userPtr = sg;
    params.edgeAntiAlias = flags & NVG_ANTIALIAS ? 1 : 0;

    ctx = nvgCreateInternal(&params);
    if (ctx == NULL) goto error;

    return ctx;

error:
    if (sg != NULL) free(sg);
    return NULL;
}

SOKOL_NANOVG_API_DECL void nvgDeleteSokol(NVGcontext* ctx) {
    nvgDeleteInternal(ctx);
}

SOKOL_NANOVG_API_DECL void snvg_set_view(NVGcontext* ctx, sg_image color_img, sg_image depth_img) {
    (void)ctx;
    (void)color_img;
    (void)depth_img;
    /* TODO: Implement custom render target support */
}

SOKOL_NANOVG_API_DECL void snvg_reset_view(NVGcontext* ctx) {
    (void)ctx;
    /* TODO: Implement custom render target support */
}

#endif /* SOKOL_NANOVG_IMPL */
