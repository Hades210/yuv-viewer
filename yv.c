#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>

#include "SDL.h"

#ifdef DEBUG
#define LOG(fmt, ...) fprintf(stderr, "%s:%d "fmt, \
                              __func__, __LINE__, __VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif
#define DIE(...)      fprintf(stderr, __VA_ARGS__)

#define COUNT_OF(a) (sizeof(a) / sizeof(a[0]))
#define precheck_range(index, array)                 \
    if (index >= sizeof(array) / sizeof(array[0])) { \
        DIE("index=%d out of range\n", index);       \
        exit(-1);                                    \
    }


/* IPC */
#define NONE 0
#define MASTER 1
#define SLAVE 2

/* IPC Commands */
#define NEXT 'a'
#define PREV 'b'
#define REW 'c'
#define ZOOM_IN 'd'
#define ZOOM_OUT 'e'
#define QUIT 'f'
#define Y_ONLY 'g'
#define CB_ONLY 'h'
#define CR_ONLY 'i'
#define ALL_PLANES 'j'

/* PROTOTYPES */
Uint32 rd(Uint8 *data, Uint32 size);
Uint32 read_planar(void);
Uint32 read_planar_vu(void);
Uint32 read_planar_vu_422sample(void);
Uint32 read_planar_vu_444sample(void);
Uint32 read_semi_planar(void);
void de_semi_planar_tile(Uint8 *data, int tiled_width, int tiled_height);
Uint32 read_semi_planar_tiled(Uint32 tw, Uint32 th);
Uint32 read_semi_planar_tiled4x4(void);
Uint32 read_semi_planar_tiled8x4(void);
Uint32 read_semi_planar_10_tiled4x4(void);
Uint32 read_semi_planar_vu(void);
Uint32 read_semi_planar_10(void);
Uint32 read_mono(void);
Uint32 read_422(void);
Uint32 read_y42210(void);
Uint32 read_yv1210(void);
Uint32 check_free_memory(void);
Uint32 allocate_memory(void);
void draw_grid422_param(int step, int dot, int color0, int color1);
void draw_grid420_param(int step, int dot, int color0, int color1);
void draw_grid422(void);
void draw_grid420(void);
Uint32 bitdepth(Uint32 fmt);
bool isPlanar(Uint32 fmt);
void luma_only(void);
void cb_only(void);
void cr_only(void);
void pre_draw(void);
void post_draw(void);
void draw_yv12(void);
void draw_422(void);
void draw_420sp(void);
Uint32 redraw(void);
Uint32 diff_mode(void);
void calc_psnr(Uint8 *frame0, Uint8 *frame1);
void usage(char *name);
void mb_loop(char *str, Uint8 *start_addr, Uint32 cols, Uint32 rows,
             Uint32 stride, Uint32 delim);
void show_mb(Uint32 mouse_x, Uint32 mouse_y);
void draw_frame(void);
Uint32 read_frame(void);
void setup_param(void);
void check_input(void);
Uint32 open_input(void);
Uint32 create_message_queue(void);
void destroy_message_queue(void);
Uint32 connect_message_queue(void);
Uint32 send_message(char cmd);
Uint32 read_message(void);
Uint32 event_dispatcher(void);
Uint32 event_loop(void);
Uint32 parse_input(int argc, char **argv);
Uint32 sdl_init(void);
Uint32 reinit(void);
void set_caption(char *array, Uint32 frame, Uint32 bytes);
void set_zoom_rect(void);
void histogram(void);
Uint32 ten2eight(Uint8 *src, Uint8 *dst, Uint32 length);
Uint32 comb_byte(Uint8 a, Uint32 offset0, Uint8 b, Uint32 offset1);
Uint32 dither(Uint32 x);
Uint32 ten2eight_compact(Uint8 *src, Uint8 *dst, Uint32 length);

Uint32 guess_arg(char *filename);
int strfmtcmp(const void *p0, const void *p1);
int strfmtcmp_r(const void *p0, const void *p1);

typedef struct StrFmt {
    char *str;
    int fmt;
} StrFmt;
StrFmt *buildStrFmtLst(Uint32 *pLen);
void destoryStrFmtLst(void);
Uint32 parse_format(char *fmtstr);
char *showFmt(Uint32 format);

/* Supported YUV-formats */
enum {
    YV12 = 0,
    IYUV = 1,
    YUY2 = 2,
    UYVY = 3,
    YVYU = 4,
    YV1210 = 5,    /* 10 bpp YV12 */
    Y42210 = 6,
    NV12 = 7,
    NV21 = 8,
    MONO = 9,
    YV16 = 10,
    YUV444P = 11,
    NV1210 = 12,
    NV12TILED = 13,
    NV1210TILED = 14,
    FORMAT_MAX,
};

typedef struct {
    int overlay_fmt;
    Uint32 (*reader)(void);
    void (*drawer)(void);
    char *fmtNameLst;
} FmtMap;

#if 0
#define SDL_YV12_OVERLAY  0x32315659  /* Planar mode: Y + V + U */
#define SDL_IYUV_OVERLAY  0x56555949  /* Planar mode: Y + U + V */
#define SDL_YUY2_OVERLAY  0x32595559  /* Packed mode: Y0+U0+Y1+V0 */
#define SDL_UYVY_OVERLAY  0x59565955  /* Packed mode: U0+Y0+V0+Y1 */
#define SDL_YVYU_OVERLAY  0x55595659  /* Packed mode: Y0+V0+Y1+U0 */
#endif
FmtMap gFmtMap[] = {
    [YV12] = {SDL_YV12_OVERLAY, read_planar, draw_yv12, "yv12 p420"},
    [IYUV] = {SDL_YV12_OVERLAY, read_planar, draw_yv12, "iyuv i420"},
    [YUY2] = {SDL_YUY2_OVERLAY, read_422, draw_422, "yuy2 yuyv"},
    [UYVY] = {SDL_UYVY_OVERLAY, read_422, draw_422, "uyvy"},
    [YVYU] = {SDL_YVYU_OVERLAY, read_422, draw_422, "yvyu"},
    [YV1210] = {SDL_YV12_OVERLAY, read_yv1210, draw_yv12, "yv1210"},
    [Y42210] = {SDL_YVYU_OVERLAY, read_y42210, draw_422, "y42210"},
    [NV12] = {SDL_YV12_OVERLAY, read_semi_planar, draw_yv12, "yuv420sp nv12"},
    [NV21] = {SDL_YV12_OVERLAY, read_semi_planar_vu, draw_yv12, "nv21"},
    [MONO] = {SDL_YV12_OVERLAY, read_mono, draw_yv12, "mono y8 grey y800"},
    [YV16] = {SDL_YV12_OVERLAY, read_planar_vu_422sample, draw_yv12, "yv16 422p"},
    [YUV444P] = {SDL_YV12_OVERLAY, read_planar_vu_444sample, draw_yv12, "444p"},
    [NV1210] = {SDL_YV12_OVERLAY, read_semi_planar_10, draw_yv12, "nv1210 yuv420sp_10bit"},
    [NV12TILED] = {SDL_YV12_OVERLAY, read_semi_planar_tiled4x4, draw_yv12, "yuv420sp_tiled"},
    [NV1210TILED] = {SDL_YV12_OVERLAY, read_semi_planar_10_tiled4x4, draw_yv12, "yuv420sp_tiled_mode0_10bit"},
};

char *showFmt(Uint32 format) {
    if (format >= FORMAT_MAX) {
        return "unknown";
    }
    return gFmtMap[format].fmtNameLst;
}

SDL_Surface *screen;
SDL_Event event;
SDL_Rect video_rect;
SDL_Overlay *my_overlay;
const SDL_VideoInfo *info = NULL;
Uint32 FORMAT = YV12;
FILE *fd;

struct my_msgbuf {
    long mtype;
    char mtext[2];
};

struct param {
    Uint32 width;             /* frame width - in pixels */
    Uint32 height;            /* frame height - in pixels */
    Uint32 wh;                /* width x height */
    Uint32 frame_size;        /* size of 1 frame - in bytes */
    Uint32 raw_frame_size;    /* original frame size - in bytes */
    Sint32 zoom;              /* zoom-factor */
    Uint32 zoom_width;
    Uint32 zoom_height;
    Uint32 grid;              /* grid-mode - on or off */
    Uint32 hist;              /* histogram-mode - on or off */
    Uint32 grid_start_pos;
    Uint32 diff;              /* diff-mode */
    Uint32 y_start_pos;       /* start pos for first Y pel */
    Uint32 cb_start_pos;      /* start pos for first Cb pel */
    Uint32 cr_start_pos;      /* start pos for first Cr pel */
    Uint32 y_only;            /* Grayscale, i.e Luma only */
    Uint32 cb_only;           /* Only Cb plane */
    Uint32 cr_only;           /* Only Cr plane */
    Uint32 mb;                /* macroblock-mode - on or off */
    Uint32 y_size;            /* sizeof luma-data for 1 frame - in bytes */
    Uint32 cb_size;           /* sizeof croma-data for 1 frame - in bytes */
    Uint32 cr_size;           /* sizeof croma-data for 1 frame - in bytes */
    Uint8 *raw;               /* pointer towards complete frame - frame_size bytes */
    Uint8 *y_data;            /* pointer towards luma-data */
    Uint8 *cb_data;           /* pointer towards croma-data */
    Uint8 *cr_data;           /* pointer towards croma-data */
    char *filename;           /* obvious */
    char *fname_diff;         /* see above */
    Uint32 overlay_format;    /* YV12, IYUV, YUY2, UYVY or YVYU - SDL */
    Uint32 vflags;            /* HW support or SW support */
    Uint8 bpp;                /* bits per pixel */
    Uint32 mode;              /* MASTER, SLAVE or NONE - defaults to NONE */
    struct my_msgbuf buf;
    int msqid;
    key_t key;
    FILE *fd2;                /* diff file */
    bool is_change_uv;        /* exchange uv status for every frame */
    bool flip_change_uv;      /* exchange uv flag this frame */
};

/* Global parameter struct */
struct param P;

Uint32 rd(Uint8 *data, Uint32 size)
{
    Uint32 cnt;

    cnt = fread(data, sizeof(Uint8), size, fd);
    if (cnt < size) {
        DIE("No more data to read!\n");
        return 0;
    }
    return 1;
}

Uint32 read_planar(void)
{
    if (!rd(P.y_data, P.y_size)) {
        return 0;
    }
    if (!rd(P.cb_data, P.cb_size)) {
        return 0;
    }
    if (!rd(P.cr_data, P.cr_size)) {
        return 0;
    }
    return 1;
}

Uint32 read_planar_vu(void)
{
    if (!rd(P.y_data, P.y_size)) {
        return 0;
    }
    if (!rd(P.cr_data, P.cr_size)) {
        return 0;
    }
    if (!rd(P.cb_data, P.cb_size)) {
        return 0;
    }
    return 1;
}

Uint32 read_planar_vu_422sample(void)
{
    if (!read_planar_vu()) {
        return 0;
    }
    // show it with YV12, 420 sample, so drop half of Cb, Cr data
    for (Uint32 i = 1; i < P.height / 2; i++) {
        memcpy(P.cr_data + i * P.width / 2, P.cr_data + i * P.width, P.width / 2);
    }
    for (Uint32 i = 1; i < P.height / 2; i++) {
        memcpy(P.cb_data + i * P.width / 2, P.cb_data + i * P.width, P.width / 2);
    }
    return 1;
}

Uint32 read_planar_vu_444sample(void)
{
    if (!read_planar_vu()) {
        return 0;
    }
    for (Uint32 i = 0; i < P.height / 2; i++) {
        for (Uint32 j = 0; j < P.width / 2; j++) {
            P.cr_data[i * P.width / 2 + j] = P.cr_data[i * P.width + j * 2];
        }
    }
    for (Uint32 i = 0; i < P.height / 2; i++) {
        for (Uint32 j = 0; j < P.width / 2; j++) {
            P.cb_data[i * P.width / 2 + j] = P.cb_data[i * P.width + j * 2];
        }
    }
    return 1;
}

Uint32 read_mono(void) {
    if (!rd(P.y_data, P.y_size)) {
        return 0;
    }
    memset(P.cb_data, 0x80, P.cb_size);
    memset(P.cr_data, 0x80, P.cr_size);
    return 1;
}

Uint32 read_semi_planar_vu(void)
{
    if (!rd(P.y_data, P.y_size)) {
        return 0;
    }

    if (!rd(P.raw, P.cb_size + P.cr_size)) {
        return 0;
    }
    Uint8 *cb = P.cb_data, *cr = P.cr_data;
    for (Uint32 i = 0; i < P.cb_size; i++) {
        *cb++ = P.raw[i * 2 + 1];
    }
    for (Uint32 i = 0; i < P.cr_size; i++) {
        *cr++ = P.raw[i * 2];
    }
    return 1;
}

Uint32 read_semi_planar(void)
{
    if (!rd(P.y_data, P.y_size)) {
        return 0;
    }

    if (!rd(P.raw, P.cb_size + P.cr_size)) {
        return 0;
    }
    Uint8 *cb = P.cb_data, *cr = P.cr_data;
    for (Uint32 i = 0; i < P.cb_size; i++) {
        *cb++ = P.raw[i * 2];
    }
    for (Uint32 i = 0; i < P.cr_size; i++) {
        *cr++ = P.raw[i * 2 + 1];
    }
    return 1;
}

Uint32 read_semi_planar_10(void)
{
    Uint32 ret = 1;
    Uint8 *data = malloc(sizeof(Uint8) * P.y_size * 1.5);
    if (!data) {
        DIE("Error allocating memory...\n");
        return 0;
    }
    if (!rd(data, P.y_size * 10 / 8)) {
        ret = 0;
        goto cleanup;
    }
    ten2eight_compact(data, P.y_data, P.y_size);

    if (!rd(data, (P.cb_size + P.cr_size) * 10 / 8)) {
        ret = 0;
        goto cleanup;
    }
    ten2eight_compact(data, P.raw, P.cb_size + P.cr_size);

    Uint8 *cb = P.cb_data, *cr = P.cr_data;
    for (Uint32 i = 0; i < P.cb_size; i++) {
        *cb++ = P.raw[i * 2];
    }
    for (Uint32 i = 0; i < P.cr_size; i++) {
        *cr++ = P.raw[i * 2 + 1];
    }

cleanup:
    free(data);
    return ret;
}

void de_semi_planar_tile(Uint8 *data, int tiled_width, int tiled_height) {
    Uint32 i, j, o, q;
    for (i = 0; i != P.height; i++) {
        for (j = 0; j != P.width; j++) {
            o = i / tiled_height * tiled_height * P.width;
            o += j / tiled_width * tiled_width * tiled_height;
            o += i % tiled_height * tiled_width;
            o += j % tiled_width;
            q = i * P.width + j;
            P.y_data[q] = data[o];

            o = i / 2 / tiled_height * tiled_height * P.width;
            o += j / tiled_width * tiled_width * tiled_height;
            o += i / 2 % tiled_height * tiled_width;
            o += j % tiled_width;
            q = i / 2 * P.width / 2 + j / 2;
            P.cr_data[q] = data[o + P.y_size];
            P.cb_data[q] = data[o + P.y_size + 1];
        }
    }
}

Uint32 read_semi_planar_tiled(Uint32 tw, Uint32 th)
{
    Uint32 size = P.frame_size;
    Uint8 *data = malloc(sizeof(Uint8) * size);
    Uint32 ret = 0;
    if (!rd(data, size)) {
        goto cleanup;
    }
    de_semi_planar_tile(data, tw, th);
    ret = 1;
cleanup:
    free(data);
    return ret;
}

Uint32 read_semi_planar_tiled4x4(void)
{
    return read_semi_planar_tiled(4, 4);
}

Uint32 read_semi_planar_tiled8x4(void)
{
    return read_semi_planar_tiled(8, 4);
}

Uint32 read_semi_planar_10_tiled4x4(void)
{
    Uint32 ret = 1;
    Uint8 *data = malloc(sizeof(Uint8) * P.y_size * 1.5);
    if (!data) {
        DIE("Error allocating memory...\n");
        return 0;
    }
    if (!rd(data, P.y_size * 10 / 8)) {
        ret = 0;
        goto cleanup;
    }
    ten2eight_compact(data, P.raw, P.y_size);
    if (!rd(data, (P.cb_size + P.cr_size) * 10 / 8)) {
        ret = 0;
        goto cleanup;
    }
    ten2eight_compact(data, P.raw + P.y_size, P.cb_size + P.cr_size);

    // now P.raw is semi_planar_tiled4x4 format
    de_semi_planar_tile(P.raw, 4, 4);
    ret = 1;
cleanup:
    free(data);
    return ret;
}

Uint32 read_422(void)
{
    Uint8 *y = P.y_data;
    Uint8 *cb = P.cb_data;
    Uint8 *cr = P.cr_data;

    if (!rd(P.raw, P.frame_size)) {
        return 0;
    }

    for (Uint32 i = P.y_start_pos; i < P.frame_size; i += 2) {
        *y++ = P.raw[i];
    }
    for (Uint32 i = P.cb_start_pos; i < P.frame_size; i += 4) {
        *cb++ = P.raw[i];
    }
    for (Uint32 i = P.cr_start_pos; i < P.frame_size; i += 4) {
        *cr++ = P.raw[i];
    }
    return 1;
}

Uint32 read_y42210(void)
{
    Uint32 ret = 1;
    Uint8 *data;
    Uint8 *tmp;

    data = malloc(sizeof(Uint8) * P.frame_size * 2);
    if (!data) {
        DIE("Error allocating memory...\n");
        return 0;
    }

    tmp = malloc(sizeof(Uint8) * P.frame_size);
    if (!tmp) {
        DIE("Error allocating memory...\n");
        ret = 0;
        goto cleany42210;
    }

    if (!rd(data, P.frame_size * 2)) {
        ret = 0;
        goto cleany42210;
    }
    ten2eight(data, tmp, P.frame_size * 2);

    /* Y  */
    for (Uint32 i = 0, j = 0; i < P.frame_size; i += 2) {
        P.raw[i] = tmp[j];
        j++;
    }
    /* Cb */
    for (Uint32 i = P.cb_start_pos, j = 0; i < P.frame_size; i += 4) {
        P.raw[i] = tmp[P.wh + j];
        j++;
    }
    /* Cr */
    for (Uint32 i = P.cr_start_pos, j = 0; i < P.frame_size; i += 4) {
        P.raw[i] = tmp[P.wh / 2 * 3 + j];
        j++;
    }

cleany42210:
    free(tmp);
    free(data);

    return ret;
}

Uint32 read_yv1210(void)
{
    Uint32 ret = 1;
    Uint8 *data;

    data = malloc(sizeof(Uint8) * P.y_size * 2);
    if (!data) {
        DIE("Error allocating memory...\n");
        return 0;
    }

    if (!rd(data, P.y_size * 2)) {
        ret = 0;
        goto cleanyv1210;
    }
    ten2eight(data, P.y_data, P.y_size * 2);

    if (!rd(data, P.cb_size * 2)) {
        ret = 0;
        goto cleanyv1210;
    }
    ten2eight(data, P.cb_data, P.cb_size * 2);

    if (!rd(data, P.cr_size * 2)) {
        ret = 0;
        goto cleanyv1210;
    }
    ten2eight(data, P.cr_data, P.cr_size * 2);

cleanyv1210:
    free(data);

    return ret;
}

Uint32 comb_byte(Uint8 a, Uint32 offset0, Uint8 b, Uint32 offset1) {
    // get 10bit from low bit to high bit
    // data sample: {0xf8, 0xe1, 0x87, 0x1f, 0x7e}
    // from low to high bit,
    // -> {0b00011111, 0b10000111, 0b11100001, 0b11111000, 0b01111110}
    // combine to 10bit
    // -> {0b0001111110, 0b0001111110, 0b0001111110, 0b0001111110}
    // round back to 8bit
    // -> {0b01111110, 0b01111110, 0b01111110, 0b01111110}
    // -> {0x7e, 0x7e, 0x7e, 0x7e}
    Uint32 x = a >> (8 - offset0);
    Uint32 y = b & ((1 << offset1) - 1);
    return (x | (y << offset0)) & 0x3ff;
}

Uint32 dither(Uint32 x) {
    return (x + 0x2) >> 2;
}

// Compact ten2eight
// Every 5 bytes representation four 10-bit data
Uint32 ten2eight_compact(Uint8 *src, Uint8 *dst, Uint32 length)
{
    Uint8 *p0, *p1;
    for (Uint32 i = 0, j = 0; j < length; i += 5, j += 4) {
        p0 = src + i;
        p1 = dst + j;
        p1[0] = dither(comb_byte(p0[0], 8, p0[1], 2));
        p1[1] = dither(comb_byte(p0[1], 6, p0[2], 4));
        p1[2] = dither(comb_byte(p0[2], 4, p0[3], 6));
        p1[3] = dither(comb_byte(p0[3], 2, p0[4], 8));
    }
    return 1;
}

// Loose ten2eight
// every two bytes representation one 10-bit data
Uint32 ten2eight(Uint8 *src, Uint8 *dst, Uint32 length)
{
    Uint16 x = 0;

    for (Uint32 i = 0; i < length; i += 2) {
        x = (src[i + 1] << 8) | src[i];
        x = (x + 2) >> 2;
        if (x > 255) {
            x = 255;
        }
        *dst++ = x;
    }

    return 1;
}

Uint32 check_free_memory(void) {
    if (P.raw != NULL) {
        free(P.raw);
        P.raw = NULL;
    }
    if (P.y_data != NULL) {
        free(P.y_data);
        P.y_data = NULL;
    }
    if (P.cb_data != NULL) {
        free(P.cb_data);
        P.cb_data = NULL;
    }
    if (P.cr_data != NULL) {
        free(P.cr_data);
        P.cr_data = NULL;
    }
    return 1;
}

Uint32 allocate_memory(void)
{
    check_free_memory();
    P.raw = malloc(sizeof(Uint8) * P.frame_size);
    P.y_data = malloc(sizeof(Uint8) * P.y_size);
    P.cb_data = malloc(sizeof(Uint8) * P.cb_size);
    P.cr_data = malloc(sizeof(Uint8) * P.cr_size);

    if (!P.raw || !P.y_data || !P.cb_data || !P.cr_data) {
        DIE("Error allocating memory...\n");
        check_free_memory();
        return 0;
    }
    return 1;
}

void draw_grid422_param(int step, int dot, int color0, int color1) {
    /* horizontal grid lines */
    for (Uint32 y = 0; y < P.height; y += step) {
        for (Uint32 x = P.grid_start_pos; x < P.width * 2; x += dot * 2) {
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x) = color0;
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x + 8) = color1;
        }
    }
    /* vertical grid lines */
    for (Uint32 x = P.grid_start_pos; x < P.width * 2; x += 2 * step) {
        for (Uint32 y = 0; y < P.height; y += dot) {
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x) = color0;
            *(my_overlay->pixels[0] + (y + 4) * my_overlay->pitches[0] + x) = color1;
        }
    }
}

void draw_grid420_param(int step, int dot, int color0, int color1) {
    /* horizontal grid lines */
    for (Uint32 y = 0; y < P.height; y += step) {
        for (Uint32 x = 0; x < P.width; x += dot) {
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x) = color0;
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x + 4) = color1;
        }
    }
    /* vertical grid lines */
    for (Uint32 x = 0; x < P.width; x += step) {
        for (Uint32 y = 0; y < P.height; y += dot) {
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x) = color0;
            *(my_overlay->pixels[0] + (y + 4) * my_overlay->pitches[0] + x) = color1;
        }
    }

}

void draw_grid422(void)
{
    if (!P.grid) {
        return;
    }
    draw_grid422_param(16, 8, 0xF0, 0x20);
    draw_grid422_param(64, 1, 0x90, 0x20);
    draw_grid422_param(256, 1, 0xE0, 0x20);
    draw_grid422_param(1024, 1, 0x00, 0x20);
}

void draw_grid420(void)
{
    if (!P.grid) {
        return;
    }
    draw_grid420_param(16, 8, 0xF0, 0x20);
    draw_grid420_param(64, 1, 0x90, 0x20);
    draw_grid420_param(256, 1, 0xE0, 0x20);
    draw_grid420_param(1024, 1, 0x00, 0x20);
}

Uint32  bitdepth(Uint32 fmt) {
    if (fmt == NV1210 || fmt == YV1210) {
        return 10;
    } else {
        return 8;
    }
}

bool isPlanar(Uint32 fmt) {
    return fmt == YV12 || fmt == IYUV || fmt == YV1210
        || fmt == NV12 || fmt == NV21 || fmt == MONO
        || fmt == YV16 || fmt == YUV444P || fmt == NV1210
        || fmt == NV12TILED || fmt == NV1210TILED;
}

void luma_only(void)
{
    if (!P.y_only) {
        return;
    }

    if (isPlanar(FORMAT)) {
        /* Set croma part to 0x80 */
        for (Uint32 i = 0; i < P.cr_size; i++) {
            my_overlay->pixels[1][i] = 0x80;
        }
        for (Uint32 i = 0; i < P.cb_size; i++) {
            my_overlay->pixels[2][i] = 0x80;
        }
        return;
    }

    /* YUY2, UYVY, YVYU */
    for (Uint32 i = P.cb_start_pos; i < P.frame_size; i += 4) {
        *(my_overlay->pixels[0] + i) = 0x80;
    }
    for (Uint32 i = P.cr_start_pos; i < P.frame_size; i += 4) {
        *(my_overlay->pixels[0] + i) = 0x80;
    }
}

void cb_only(void)
{
    if (!P.cb_only || FORMAT == MONO) {
        return;
    }

    if (isPlanar(FORMAT)) {
        /* Set Luma part and Cr to 0x80 */
        for (Uint32 i = 0; i < P.y_size; i++) {
            my_overlay->pixels[0][i] = 0x80;
        }
        for (Uint32 i = 0; i < P.cr_size; i++) {
            my_overlay->pixels[1][i] = 0x80;
        }
        return;
    }

    /* YUY2, UYVY, YVYU */
    for (Uint32 i = P.y_start_pos; i < P.frame_size; i += 2) {
        *(my_overlay->pixels[0] + i) = 0x80;
    }
    for (Uint32 i = P.cr_start_pos; i < P.frame_size; i += 4) {
        *(my_overlay->pixels[0] + i) = 0x80;
    }
}

void cr_only(void)
{
    if (!P.cr_only || FORMAT == MONO) {
        return;
    }

    if (isPlanar(FORMAT)) {
        /* Set Luma part and Cb to 0x80 */
        for (Uint32 i = 0; i < P.y_size; i++) {
            my_overlay->pixels[0][i] = 0x80;
        }
        for (Uint32 i = 0; i < P.cb_size; i++) {
            my_overlay->pixels[2][i] = 0x80;
        }
        return;
    }

    /* YUY2, UYVY, YVYU */
    for (Uint32 i = P.y_start_pos; i < P.frame_size; i += 2) {
        *(my_overlay->pixels[0] + i) = 0x80;
    }
    for (Uint32 i = P.cb_start_pos; i < P.frame_size; i += 4) {
        *(my_overlay->pixels[0] + i) = 0x80;
    }
}

#define SWAP(a, b, T)      { T t = a; a = b; b = t; }
void pre_draw(void) {
    if (P.flip_change_uv) {
        P.flip_change_uv = false;
        P.is_change_uv = !P.is_change_uv;
        // exchange UV planar for this frame
        SWAP(P.cr_data, P.cb_data, unsigned char *);
    } else if (P.is_change_uv) {
        // keep exchange UV for next frames
        SWAP(P.cr_data, P.cb_data, unsigned char *);
    }
}

void post_draw(void) {
    luma_only();
    cb_only();
    cr_only();
    histogram();
}

void draw_420sp(void) {
    pre_draw();
    memcpy(my_overlay->pixels[0], P.y_data, P.y_size);
    memcpy(my_overlay->pixels[1], P.cb_data, P.cb_size);
    memcpy(my_overlay->pixels[2], P.cr_data, P.cr_size);
    draw_grid420();
    post_draw();
}

void draw_yv12(void)
{
    pre_draw();
    memcpy(my_overlay->pixels[0], P.y_data, P.y_size);
    memcpy(my_overlay->pixels[1], P.cr_data, P.cr_size);
    memcpy(my_overlay->pixels[2], P.cb_data, P.cb_size);
    draw_grid420();
    post_draw();
}

void draw_422(void)
{
    pre_draw();
    memcpy(my_overlay->pixels[0], P.raw, P.frame_size);
    draw_grid422();
    post_draw();
}

void usage(char *name)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "%s filename [width height format [diff_filename]]\n", name);
    fprintf(stderr, "\twhen only have filename arg,"
            " try guess other arg from filename\n");
    fprintf(stderr, "\tformat=[");
    char *s;
    for (Uint32 i = 0; i != COUNT_OF(gFmtMap); i++) {
        if (i == 0) {
            s = "%s";
        } else {
            s = ", %s";
        }
        fprintf(stderr, s, showFmt(i));
    }
    fprintf(stderr, "]\n");
}

// show block size cols, rows, stride,
// add delim width to show YUYV alike case.
void mb_loop(char *str, Uint8 *start_addr, Uint32 cols, Uint32 rows,
             Uint32 stride, Uint32 delim)
{
    printf("%s\n", str);
    for (Uint32 i = 0; i < rows; i++) {
        for (Uint32 j = 0; j < cols; j++) {
            printf("%02X ", start_addr[i * stride + j * (1 + delim)]);
            if ((j + 1) % 4 == 0) {
                printf(" ");
            }
        }
        printf("\n");
        if ((i + 1) % 4 == 0) {
            printf("\n");
        }
    }
}

void show_mb(Uint32 mouse_x, Uint32 mouse_y)
{
    if (!P.mb) {
        return;
    }
    if (bitdepth(FORMAT) == 10) {
        printf("10-bitdepth raw data dither to 8bit\n");
    }
    if (P.width % 16 != 0) {
        printf("support non-align width=%d have issue\n", P.width);
        goto unsupport;
    }
    Uint32 MB;

    /* which MB are we in? */
    int mb_x = mouse_x / (16 * P.zoom);
    int mb_y = mouse_y / (16 * P.zoom);
    MB = mb_x + (P.width / 16) * mb_y;

    printf("\nMB (%d, %d) #%d\n", mb_x, mb_y, MB);

    void (*drawer)(void) = gFmtMap[FORMAT].drawer;
    if (drawer == draw_422) {
        Uint8 *p = P.raw + 32 * MB;
        switch (gFmtMap[FORMAT].overlay_fmt) {
            case SDL_YUY2_OVERLAY:
                /* Packed mode: Y0+U0+Y1+V0 */
                mb_loop("= Y =", p, 16, 16, P.width, 1);
                mb_loop("= Cb =", p + 1, 8, 16, P.width, 3);
                mb_loop("= Cr =", p + 3, 8, 16, P.width, 3);
                break;
            case SDL_UYVY_OVERLAY:
                mb_loop("= Y =", p + 1, 16, 16, P.width, 1);
                mb_loop("= Cb =", p, 8, 16, P.width, 3);
                mb_loop("= Cr =", p + 2, 8, 16, P.width, 3);
                break;
            case SDL_YVYU_OVERLAY:
                mb_loop("= Y =", p, 16, 16, P.width, 1);
                mb_loop("= Cb =", p + 3, 8, 16, P.width, 3);
                mb_loop("= Cr =", p + 1, 8, 16, P.width, 3);
                break;
            default:
                goto unsupport;
        }
    } else if (drawer == draw_yv12) {
        mb_loop("= Y =", P.y_data + 16 * MB, 16, 16, P.width, 0);
        mb_loop("= Cb =", P.cb_data + 8 * MB, 8, 8, P.width / 2, 0);
        mb_loop("= Cr =", P.cr_data + 8 * MB, 8, 8, P.width / 2, 0);
    } else {
        goto unsupport;
    }

    fflush(stdout);
    return;
unsupport:
    printf("unsupport show_mb for format=%d[%s]\n",
           FORMAT, gFmtMap[FORMAT].fmtNameLst);
}

void draw_frame(void)
{
    // lock pixels before modifying them
    SDL_LockYUVOverlay(my_overlay);
    precheck_range(FORMAT, gFmtMap);
    (gFmtMap[FORMAT].drawer)();
    set_zoom_rect();
    video_rect.x = 0;
    video_rect.y = 0;
    video_rect.w = P.zoom_width;
    video_rect.h = P.zoom_height;
    SDL_UnlockYUVOverlay(my_overlay);

    SDL_DisplayYUVOverlay(my_overlay, &video_rect);
}

Uint32 read_frame(void)
{
    if (!P.diff) {
        precheck_range(FORMAT, gFmtMap);
        return (gFmtMap[FORMAT].reader)();
    } else {
        return diff_mode();
    }
}

Uint32 diff_mode(void)
{
    FILE *fd_tmp;
    Uint8 *y_tmp;

    /* Perhaps a bit ugly but it seams to work...
     * 1. read frame from fd
     * 2. store data away
     * 3. read frame from P.fd2
     * 4. calculate diff
     * 5. place result in P.raw or P.y_data depending on FORMAT
     * 6. diff works on luma data so clear P.cb_data and P.cr_data
     * Fiddle with the file descriptors so that we read
     * from correct file.
     */

    precheck_range(FORMAT, gFmtMap);
    if (!(gFmtMap[FORMAT].reader)()) {
        return 0;
    }

    y_tmp = malloc(sizeof(Uint8) * P.y_size);

    if (!y_tmp) {
        DIE("Error allocating memory...\n");
        return 0;
    }

    for (Uint32 i = 0; i < P.y_size; i++) {
        y_tmp[i] = P.y_data[i];
    }

    fd_tmp = fd;
    fd = P.fd2;

    precheck_range(FORMAT, gFmtMap);
    if (!(gFmtMap[FORMAT].reader)()) {
        free(y_tmp);
        fd = fd_tmp;
        return 0;
    }

    /* restore file descriptor */
    fd = fd_tmp;

    /* now, P.y_data contains luminance data for fd2 and
     * y_tmp contains luma data for fd.
     * Calculate diff and place result where it belongs
     * Clear croma data */

    calc_psnr(y_tmp, P.y_data);

    if (FORMAT == YV12 || FORMAT == IYUV) {
        for (Uint32 i = 0; i < P.y_size; i++) {
            P.y_data[i] = 0x80 - (y_tmp[i] - P.y_data[i]);
        }
        for (Uint32 i = 0; i < P.cb_size; i++) {
            P.cb_data[i] = 0x80;
        }
        for (Uint32 i = 0; i < P.cr_size; i++) {
            P.cr_data[i] = 0x80;
        }
    } else {
        Uint32 j = 0;
        for (Uint32 i = P.y_start_pos; i < P.frame_size; i += 2) {
            P.raw[i] = 0x80 - (y_tmp[j] - P.y_data[j]);
            j++;
        }
        for (Uint32 i = P.cb_start_pos; i < P.frame_size; i += 4) {
            P.raw[i] = 0x80;
        }
        for (Uint32 i = P.cr_start_pos; i < P.frame_size; i += 4) {
            P.raw[i] = 0x80;
        }
    }

    free(y_tmp);

    return 1;
}

void calc_psnr(Uint8 *frame0, Uint8 *frame1)
{
    double mse = 0.0;
    double mse_tmp = 0.0;
    double psnr = 0.0;

    // only compare Y component
    for (Uint32 i = 0; i < P.y_size; i++) {
        mse_tmp = abs(frame0[i] - frame1[i]);
        mse += mse_tmp * mse_tmp;
    }

    /* division by zero */
    if (mse == 0) {
        fprintf(stdout, "PSNR: NaN MSE=0\n");
        return;
    }

    mse /= P.y_size;

    psnr = 10.0 * log10((256 * 256) / mse);

    fprintf(stdout, "PSNR: %f\n", psnr);
}

void histogram(void)
{
    if (!P.hist) {
        return;
    }

    Uint8 y[256] = {0};
    Uint8 b[256] = {0};
    Uint8 r[256] = {0};

    for (Uint32 i = 0; i < P.y_size; i++) {
        y[P.y_data[i]]++;
    }
    for (Uint32 i = 0; i < P.cb_size; i++) {
        b[P.cb_data[i]]++;
    }
    for (Uint32 i = 0; i < P.cr_size; i++) {
        r[P.cr_data[i]]++;
    }

    fprintf(stdout, "\nY,");
    for (Uint32 i = 0; i < 256; i++) {
        fprintf(stdout, "%u,", y[i]);
    }
    fprintf(stdout, "\nCb,");
    for (Uint32 i = 0; i < 256; i++) {
        fprintf(stdout, "%u,", b[i]);
    }
    fprintf(stdout, "\nCr,");
    for (Uint32 i = 0; i < 256; i++) {
        fprintf(stdout, "%u,", r[i]);
    }
    fprintf(stdout, "\n");
    fflush(stdout);
}

void setup_param(void)
{
    P.zoom = 1;
    P.wh = P.width * P.height;

    switch (FORMAT) {
        case YV12:
        case IYUV:
        case YV1210:
        case NV12:
        case NV1210:
        case NV21:
        case NV12TILED:
        case NV1210TILED:
        case MONO:
            P.y_size = P.wh;
            P.cb_size = P.cr_size = P.wh / 4;
            break;
        case YUY2:
        case UYVY:
        case YVYU:
        case Y42210:
        case YV16:
            P.y_size = P.wh;
            P.cb_size = P.cr_size = P.wh / 2;
            P.grid_start_pos = 0;
            break;
        case YUV444P:
            P.y_size = P.cb_size = P.cr_size = P.wh;
            break;
        default:
            DIE("unhandled format=%d(%s)\n", FORMAT, showFmt(FORMAT));
            break;
    }
    P.frame_size = P.y_size + P.cb_size + P.cr_size;
    P.raw_frame_size = P.frame_size * bitdepth(FORMAT) / 8;

    if (FORMAT == YUY2) {
        /* Y U Y V
         * 0 1 2 3 */
        P.y_start_pos = 0;
        P.cb_start_pos = 1;
        P.cr_start_pos = 3;
    } else if (FORMAT == UYVY) {
        /* U Y V Y
         * 0 1 2 3 */
        P.y_start_pos = 1;
        P.grid_start_pos = 1;
        P.cb_start_pos = 0;
        P.cr_start_pos = 2;
    } else if (FORMAT == YVYU || FORMAT == Y42210) {
        /* Y V Y U
         * 0 1 2 3 */
        P.y_start_pos = 0;
        P.cb_start_pos = 3;
        P.cr_start_pos = 1;
    }
    printf("format=%d size=%dx%d frame_size=%d y_size=%d cb_size=%d cr_size=%d\n",
           FORMAT, P.width, P.height, P.frame_size, P.y_size, P.cb_size, P.cr_size);
}

void check_input(void)
{
    Uint32 file_size;

    /* Frame Size is an even multipe of 16x16? */
    if (P.width % 16 != 0) {
        DIE("WIDTH not multiple of 16, check input...\n");
    }
    if (P.height % 16 != 0) {
        DIE("HEIGHT not multiple of 16, check input...\n");
    }

    /* Even number of frames? */
    fseek(fd, 0L, SEEK_END);
    file_size = ftell(fd);
    fseek(fd, 0L, SEEK_SET);

    if (file_size % P.frame_size != 0) {
        DIE("#FRAMES not an integer, check input...\n");
    }
}

Uint32 create_message_queue(void)
{
    /* Should probably use argv[0] or similar as pathname
     * when creating the key;
     * but let's keep it simple for now. Y for YCbCr
     */
    if ((P.key = ftok("/tmp", 'Y')) == -1) {
        perror("ftok");
        return 0;
    }

    if ((P.msqid = msgget(P.key, 0644 | IPC_CREAT)) == -1) {
        perror("msgget");
        return 0;
    }

    /* we don't really care in this case */
    P.buf.mtype = 1;
    return 1;
}

Uint32 connect_message_queue(void)
{
    if ((P.key = ftok("/tmp", 'Y')) == -1) {
        perror("ftok");
        return 0;
    }

    /* connect to the queue */
    if ((P.msqid = msgget(P.key, 0644)) == -1) {
        perror("msgget");
        return 0;
    }

    printf("Ready to receive messages, captain.\n");
    return 1;
}

Uint32 send_message(char cmd)
{
    if (P.mode != MASTER) {
        return 1;
    }

    P.buf.mtext[0] = cmd;
    P.buf.mtext[1] = '\0';

    if (msgsnd(P.msqid, &P.buf, 2, 0) == -1) {
        perror("msgsnd");
        return 0;
    }

    return 1;
}

Uint32 read_message(void)
{
    if (msgrcv(P.msqid, &P.buf, sizeof(P.buf.mtext), 0, 0) == -1) {
        perror("msgrcv");
        P.mode = NONE;
        return 0;
    }

    return 1;
}

void destroy_message_queue(void)
{
    if (P.mode == MASTER) {
        if (msgctl(P.msqid, IPC_RMID, NULL) == -1) {
            perror("msgctl");
        }
        printf("queue destroyed\n");
    }
}

Uint32 event_dispatcher(void)
{
    if (read_message()) {

        switch (P.buf.mtext[0]) {
            case NEXT:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_RIGHT;
                SDL_PushEvent(&event);
                break;

            case PREV:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_LEFT;
                SDL_PushEvent(&event);
                break;

            case REW:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_r;
                SDL_PushEvent(&event);
                break;

            case ZOOM_IN:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_UP;
                SDL_PushEvent(&event);
                break;

            case ZOOM_OUT:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_DOWN;
                SDL_PushEvent(&event);
                break;

            case QUIT:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_q;
                SDL_PushEvent(&event);
                break;

            case Y_ONLY:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_F5;
                SDL_PushEvent(&event);
                break;

            case CB_ONLY:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_F6;
                SDL_PushEvent(&event);
                break;

            case CR_ONLY:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_F7;
                SDL_PushEvent(&event);
                break;

            case ALL_PLANES:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_F8;
                SDL_PushEvent(&event);
                break;

            default:
                DIE("~TILT\n");
                return 0;
        }
    }
    return 1;
}

void set_caption(char *array, Uint32 frame, Uint32 bytes)
{
    snprintf(array, bytes, "%s - %s%s%s%s%s%s%s%s frame %d, size %dx%d",
             P.filename,
             (P.mode == MASTER) ? "[MASTER]" :
             (P.mode == SLAVE) ? "[SLAVE]" : "",
             P.grid ? "G" : "",
             P.mb ? "M" : "",
             P.diff ? "D" : "",
             P.hist ? "H" : "",
             P.y_only ? "Y" : "",
             P.cb_only ? "Cb" : "",
             P.cr_only ? "Cr" : "",
             frame,
             P.zoom_width,
             P.zoom_height);
}

void set_zoom_rect(void)
{
    if (P.zoom > 0) {
        P.zoom_width = P.width * P.zoom;
        P.zoom_height = P.height * P.zoom;
    } else if (P.zoom <= 0) {
        P.zoom_width = P.width / (abs(P.zoom) + 2);
        P.zoom_height = P.height / (abs(P.zoom) + 2);
    } else {
        DIE("ERROR in zoom:\n");
    }
    // printf("zoom to %dx%d\n", P.zoom_width, P.zoom_height);
}

Uint32 redraw(void)
{
    fseek(fd, 0, SEEK_SET);
    if (P.diff) {
        fseek(P.fd2, 0, SEEK_SET);
    }
    read_frame();
    draw_frame();
    send_message(REW);
    return 1;
}

Uint32 reinit(void)
{
    /* Initialize parameters corresponding to YUV-format */
    setup_param();
    if (!sdl_init()) {
        return 0;
    }
    if (!allocate_memory()) {
        return 0;
    }
    return 1;
}

/* loop inspired by yay
 * http://freecode.com/projects/yay
 */
Uint32 event_loop(void)
{
    char caption[256];
    Uint16 quit = 0;
    Uint32 frame = 0;
    int play_yuv = 0;
    unsigned int start_ticks = 0;

    while (!quit) {

        set_caption(caption, frame, 256);
        SDL_WM_SetCaption(caption, NULL);

        /* wait for SDL event */
        if (P.mode == NONE || P.mode == MASTER) {
            SDL_WaitEvent(&event);
        } else if (P.mode == SLAVE) {
            if (!event_dispatcher()) {
                SDL_WaitEvent(&event);
            }
        }

        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_SPACE:
                        play_yuv = 1; /* play it, sam! */
                        while (play_yuv) {
                            start_ticks = SDL_GetTicks();
                            set_caption(caption, frame, 256);
                            SDL_WM_SetCaption(caption, NULL);

                            /* check for next frame existing */
                            if (read_frame()) {
                                draw_frame();
                                /* insert delay for real time viewing */
                                if (SDL_GetTicks() - start_ticks < 40) {
                                    SDL_Delay(40 - (SDL_GetTicks() - start_ticks));
                                }
                                frame++;
                                send_message(NEXT);
                            } else {
                                play_yuv = 0;
                            }
                            /* check for any key event */
                            if (SDL_PollEvent(&event)) {
                                if (event.type == SDL_KEYDOWN) {
                                    /* stop playing */
                                    play_yuv = 0;
                                }
                            }
                        }
                        break;
                    case SDLK_RIGHT: /* next frame */
                        /* check for next frame existing */
                        if (read_frame()) {
                            draw_frame();
                            frame++;
                            send_message(NEXT);
                        }
                        break;
                    case SDLK_LEFT: /* previous frame */
                        if (frame > 1) {
                            frame--;
                            fseek(fd, ((frame - 1) * P.raw_frame_size), SEEK_SET);
                            if (P.diff) {
                                fseek(P.fd2, ((frame - 1) * P.raw_frame_size), SEEK_SET);
                            }
                            read_frame();
                            draw_frame();
                            send_message(PREV);
                        }
                        break;
                    case SDLK_UP: /* zoom in */
                        P.zoom++;
                        set_zoom_rect();
                        screen = SDL_SetVideoMode(P.zoom_width,
                                                  P.zoom_height,
                                                  P.bpp, P.vflags);
                        video_rect.w = P.zoom_width;
                        video_rect.h = P.zoom_height;
                        SDL_DisplayYUVOverlay(my_overlay, &video_rect);
                        send_message(ZOOM_IN);
                        break;
                    case SDLK_DOWN: /* zoom out */
                        P.zoom--;
                        set_zoom_rect();
                        screen = SDL_SetVideoMode(P.zoom_width,
                                                  P.zoom_height,
                                                  P.bpp, P.vflags);
                        video_rect.w = P.zoom_width;
                        video_rect.h = P.zoom_height;
                        SDL_DisplayYUVOverlay(my_overlay, &video_rect);
                        send_message(ZOOM_OUT);
                        break;
                    case SDLK_r: /* rewind */
                        if (frame > 1) {
                            frame = 1;
                            redraw();
                        }
                        break;
                    case SDLK_l:
                        P.width += 16;
                        frame = 1; reinit(); redraw(); break;
                    case SDLK_h:
                        P.width -= 16;
                        frame = 1; reinit(); redraw(); break;
                    case SDLK_j:
                        P.height += 16;
                        frame = 1; reinit(); redraw(); break;
                    case SDLK_k:
                        P.height -= 16;
                        frame = 1; reinit(); redraw(); break;
                    case SDLK_g: /* display grid */
                        P.grid = ~P.grid;
                        if (P.zoom < 1) {
                            P.grid = 0;
                        }
                        draw_frame();
                        break;
                    case SDLK_x:
                        P.flip_change_uv = true;
                        draw_frame();
                        break;
                    case SDLK_m: /* show mb-data on stdout */
                        P.mb = ~P.mb;
                        if (P.zoom < 1) {
                            P.mb = 0;
                        }
                        draw_frame();
                        break;
                    case SDLK_F5: /* Luma data only */
                    case SDLK_y:
                        P.y_only = ~P.y_only;
                        P.cb_only = 0;
                        P.cr_only = 0;
                        draw_frame();
                        send_message(Y_ONLY);
                        break;
                    case SDLK_F6: /* Cb data only */
                    case SDLK_u:
                        P.cb_only = ~P.cb_only;
                        P.y_only = 0;
                        P.cr_only = 0;
                        draw_frame();
                        send_message(CB_ONLY);
                        break;
                    case SDLK_F7: /* Cr data only */
                    case SDLK_v:
                        P.cr_only = ~P.cr_only;
                        P.y_only = 0;
                        P.cb_only = 0;
                        send_message(CR_ONLY);
                        draw_frame();
                        break;
                    case SDLK_F8: /* display all color planes */
                    case SDLK_a:
                        P.y_only = 0;
                        P.cb_only = 0;
                        P.cr_only = 0;
                        draw_frame();
                        send_message(ALL_PLANES);
                        break;
                    case SDLK_s: /* histogram */
                        P.hist = ~P.hist;
                        draw_frame();
                        break;
                    case SDLK_F1: /* MASTER-mode */
                        if (create_message_queue()) {
                            P.mode = MASTER;
                        }
                        break;
                    case SDLK_F2: /* SLAVE-mode */
                        if (P.mode == MASTER) {
                            destroy_message_queue();
                        }
                        if (connect_message_queue()) {
                            P.mode = SLAVE;
                        }
                        break;
                    case SDLK_F3: /* NONE-mode */
                        destroy_message_queue();
                        P.mode = NONE;
                        break;
                    case SDLK_q: /* quit */
                        quit = 1;
                        send_message(QUIT);
                        break;
                    default:
                        break;
                } /* switch key */
                break;
            case SDL_QUIT:
                quit = 1;
                break;
            case SDL_VIDEOEXPOSE:
                SDL_DisplayYUVOverlay(my_overlay, &video_rect);
                break;
            case SDL_MOUSEBUTTONDOWN:
                /* If the left mouse button was pressed */
                if (event.button.button == SDL_BUTTON_LEFT ) {
                    show_mb(event.button.x, event.button.y);
                }
                break;

            default:
                break;

        } /* switch event type */

    } /* while */

    return quit;
}

int strfmtcmp(const void *p0, const void *p1) {
    char *s0 = ((StrFmt *)p0)->str;
    char *s1 = ((StrFmt *)p1)->str;
    int l0 = strlen(s0);
    int l1 = strlen(s1);
    if (l0 != l1) {
        return l0 - l1;
    }
    return strcmp(s0, s1);
}

int strfmtcmp_r(const void *p0, const void *p1) {
    return -1 * strfmtcmp(p0, p1);
}

static char **gNameLst;
StrFmt *buildStrFmtLst(Uint32 *pLen) {
    // fmtnameLst is a list with whitespace delimiter
    // so split it to list.
    // and build one dict which map from format name to format
    Uint32 ft = 0, len = COUNT_OF(gFmtMap);
    gNameLst = malloc(sizeof(char *) * len);
    for (ft = 0; ft != len; ft++) {
        // dup it for writeable string
        gNameLst[ft] = strdup(showFmt(ft));
    }

    int toklen = 2 * len, tokidx;
    char delim[] = " ";
    char *s, *token;
    StrFmt *toklst = malloc(sizeof(StrFmt) * toklen);
    memset(toklst, 0, sizeof(StrFmt) * toklen);
    for (ft = tokidx = 0; ft != len; ft++) {
        s = gNameLst[ft];
        while ((token = strtok(s, delim)) != NULL) {
            s = NULL;
            toklst[tokidx].str = token;
            toklst[tokidx].fmt = ft;
            tokidx++;
            if (tokidx > toklen) {
                DIE("not enough toklst, toklen=%d\n", toklen);
                goto cleanup;
            }
        }
    }
    *pLen = tokidx;
    return toklst;
cleanup:
    *pLen = 0;
    free(toklst);
    return NULL;
}

void destoryStrFmtLst() {
    for (int ft = 0; ft != COUNT_OF(gFmtMap); ft++) {
        free(gNameLst[ft]);
    }
}

Uint32 parse_format(char *fmtstr) {
    FORMAT = FORMAT_MAX;
    Uint32 tokidx;
    StrFmt *toklst = buildStrFmtLst(&tokidx);
    // sort it for longest match
    qsort(toklst, tokidx, sizeof(StrFmt), strfmtcmp_r);
    for (Uint32 i = 0; i != tokidx; i++) {
        StrFmt *p = toklst + i;
        if (strcasestr(fmtstr, p->str) != NULL) {
            FORMAT = p->fmt;
            goto cleanup;
        }
    }

cleanup:
    destoryStrFmtLst();
    return FORMAT != FORMAT_MAX;
}

Uint32 negCondSkip(char *s, int (*cond)(int));
Uint32 negCondSkip(char *s, int (*cond)(int)) {
    int i;
    for (i = 0; s[i] != '\0' && !cond(s[i]); i++) {
    }
    return i;
}

Uint32 guess_arg(char *filename) {
    /* how to guess
     * - first number for width
     * - second number for height
     * - longest match format string in whole filename
     */
    char *s = filename;
    char *p = s;
    p += negCondSkip(p, isdigit);
    if (*p == '\0') {
        DIE("cannot find width\n");
        return 0;
    }
    int width = strtol(p, &p, 10);

    p += negCondSkip(p, isdigit);
    if (*p == '\0') {
        DIE("cannot find height\n");
        return 0;
    }
    int height = strtol(p, &p, 10);

    parse_format(filename);
    if (FORMAT == FORMAT_MAX) {
        DIE("cannot find fmt\n");
        return 0;
    }

    P.width = width;
    P.height = height;
    LOG("guess width=%d height=%d fmt=%d(%s)\n",
        width, height, FORMAT, showFmt(FORMAT));
    return 1;
}

Uint32 parse_input(int argc, char **argv)
{
    if (argc == 2 && guess_arg(argv[1])) {
        P.filename = argv[1];
    } else if (argc == 5 || argc == 6) {
        if (argc == 6) {
            /* diff mode */
            P.diff = 1;
            P.fname_diff = argv[5];
            printf("diff mode: with fn=[%s]\n", P.fname_diff);
        }

        P.filename = argv[1];
        P.width = atoi(argv[2]);
        P.height = atoi(argv[3]);
        char *fmt = argv[4];
        parse_format(fmt);
        if (FORMAT == FORMAT_MAX) {
            DIE("The format option '%s' is not recognized\n", fmt);
            return 0;
        }
    } else {
        usage(argv[0]);
        return 0;
    }
    precheck_range(FORMAT, gFmtMap);
    P.overlay_format = gFmtMap[FORMAT].overlay_fmt;
    char *cc = (char *)&P.overlay_format;
    printf("arg %dx%d FORMAT=%d(%s) show overlay_format=%#x(%c%c%c%c)\n",
           P.width, P.height, FORMAT, showFmt(FORMAT),
           P.overlay_format, cc[0], cc[1], cc[2], cc[3]);
    return 1;
}

Uint32 open_input(void)
{
    fd = fopen(P.filename, "rb");
    if (fd == NULL) {
        DIE("Error opening file=%s\n", P.filename);
        return 0;
    }

    if (P.diff) {
        P.fd2 = fopen(P.fname_diff, "rb");
        if (P.fd2 == NULL) {
            DIE("Error opening %s\n", P.fname_diff);
            return 0;
        }
    }
    return 1;
}

Uint32 sdl_init(void)
{
    /* SDL init */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        DIE("Unable to set video mode: %s\n", SDL_GetError());
        atexit(SDL_Quit);
        return 0;
    }

    info = SDL_GetVideoInfo();
    if (!info) {
        DIE("SDL ERROR Video query failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    P.bpp = info->vfmt->BitsPerPixel;

    if (info->hw_available) {
        P.vflags = SDL_HWSURFACE;
    } else {
        P.vflags = SDL_SWSURFACE;
    }

    // find SDL_SetVideoMode crash when 32768x320 size
    if (P.width > 4096 || P.height > 2176) {
        DIE("SDL cannot support size=%dx%d > 4096x2160\n", P.width, P.height);
        return 0;
    }

    if ((screen = SDL_SetVideoMode(P.width, P.height, P.bpp, P.vflags)) == 0) {
        DIE("SDL ERROR Video mode set failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    my_overlay = SDL_CreateYUVOverlay(P.width, P.height, P.overlay_format, screen);
    if (!my_overlay) {
        DIE("Couldn't create overlay\n");
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    int ret = EXIT_SUCCESS;

    /* Initialize param struct to zero */
    memset(&P, 0, sizeof(P));

    if (!parse_input(argc, argv)) {
        return EXIT_FAILURE;
    }

    if (!open_input()) {
        return EXIT_FAILURE;
    }

    if (!reinit()) {
        goto cleanup;
    }

    /* Lets do some basic consistency check on input */
    check_input();

    /* send event to display first frame */
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_RIGHT;
    SDL_PushEvent(&event);

    /* while true */
    event_loop();

cleanup:
    destroy_message_queue();
    SDL_FreeYUVOverlay(my_overlay);
    check_free_memory();
    if (fd) {
        fclose(fd);
    }
    if (P.fd2) {
        fclose(P.fd2);
    }

    return ret;
}
