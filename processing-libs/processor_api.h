#ifndef PROCESSOR_API_H
#define PROCESSOR_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>  /* for uint32_t, uint8_t */

typedef enum {
    PROC_STATUS_OK          = 0,
    PROC_STATUS_ERR_GENERAL = -1,
    PROC_STATUS_ERR_CONFIG  = -2,
    PROC_STATUS_ERR_ALLOC   = -3,
    PROC_STATUS_ERR_RUNTIME = -4
} ProcStatus;

typedef enum {
    PROC_PIXFMT_UNKNOWN = 0,
    PROC_PIXFMT_GRAY8   = 1,
    PROC_PIXFMT_RGB24   = 2,
    PROC_PIXFMT_RGBA32  = 3
    /* add more as needed */
} ProcPixelFormat;

typedef struct {
    int width;              /* nominal frame width  (pixels) */
    int height;             /* nominal frame height (pixels) */
    ProcPixelFormat pixfmt; /* expected pixel format */
    const char* config_path;

//    int reserved[4];
} VP_Config;

typedef struct {
    int width;
    int height;
    int stride;              /* bytes per row */
    const uint8_t* data;     /* read-only input buffer */
} VP_FrameIn;

typedef struct {
    int width;
    int height;
    int stride;              /* bytes per row */
    uint8_t* data;           /* caller-allocated output buffer */
} VP_FrameOut;

typedef struct ProcessorAPI {
    /* Create a new processor instance. The implementation allocates *ctx. */
    ProcStatus (*init)(const VP_Config* cfg, void** ctx);

    /* Process one frame. ctx is the instance state. */
    ProcStatus (*process)(void* ctx,
                          const VP_FrameIn* in_frame,
                          VP_FrameOut* out_frame);

    /* Destroy a processor instance created by init(). */
    void (*destroy)(void* ctx);

    /* Room for future extension without breaking ABI */
    void* reserved[4];
} ProcessorAPI;

ProcStatus proc_register(ProcessorAPI* api);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PROCESSOR_API_H */
