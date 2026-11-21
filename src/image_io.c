/* SPDX-License-Identifier: MIT */
#include "rts/gfx.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include <string.h>

bool rts_image_load_png_memory(const uint8_t *bytes, size_t size, RtsImage *out_img) {
    if (!bytes || size == 0 || !out_img) {
        return false;
    }
    memset(out_img, 0, sizeof(*out_img));
    int w = 0, h = 0, ch = 0;
    unsigned char *px = stbi_load_from_memory(bytes, (int)size, &w, &h, &ch, 4);
    if (!px) {
        return false;
    }
    out_img->width = w;
    out_img->height = h;
    out_img->channels = 4;
    out_img->pixels = px;
    return true;
}

void rts_image_free(RtsImage *img) {
    if (!img) return;
    if (img->pixels) {
        stbi_image_free(img->pixels);
        img->pixels = NULL;
    }
    img->width = 0;
    img->height = 0;
    img->channels = 0;
}

bool rts_image_write_png(const char *path, int width, int height, int channels, const uint8_t *pixels) {
    if (!path || !pixels || width <= 0 || height <= 0 || channels <= 0) {
        return false;
    }
    return stbi_write_png(path, width, height, channels, pixels, width * channels) != 0;
}
