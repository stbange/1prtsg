/* SPDX-License-Identifier: MIT */
#include "rts/gfx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Vec2f {
    float u, v;
} Vec2f;

static bool parse_face_token(const char *tok, int *vi, int *ti, int *ni) {
    *vi = 0;
    *ti = 0;
    *ni = 0;
    if (sscanf(tok, "%d/%d/%d", vi, ti, ni) == 3) return true;
    if (sscanf(tok, "%d//%d", vi, ni) == 2) return true;
    if (sscanf(tok, "%d/%d", vi, ti) == 2) return true;
    if (sscanf(tok, "%d", vi) == 1) return true;
    return false;
}

static bool grow_buf(void **ptr, size_t *cap, size_t elem_sz, size_t needed) {
    if (needed <= *cap) return true;
    size_t ncap = (*cap == 0) ? 64u : (*cap * 2u);
    while (ncap < needed) ncap *= 2u;
    void *nb = realloc(*ptr, ncap * elem_sz);
    if (!nb) return false;
    *ptr = nb;
    *cap = ncap;
    return true;
}

bool rts_obj_load_memory(const char *obj_text, RtsVec3f tint, RtsObjMesh *out_mesh) {
    if (!obj_text || !out_mesh) {
        return false;
    }
    memset(out_mesh, 0, sizeof(*out_mesh));

    RtsVec3f *pos = NULL; size_t pos_n = 0, pos_cap = 0;
    Vec2f *uvs = NULL;    size_t uv_n = 0,  uv_cap = 0;
    RtsVec3f *nrm = NULL; size_t nrm_n = 0, nrm_cap = 0;
    RtsVertex *out = NULL; size_t out_n = 0, out_cap = 0;

    const char *p = obj_text;
    char line[512];
    while (*p != '\0') {
        size_t len = 0;
        while (p[len] && p[len] != '\n' && len < sizeof(line) - 1) {
            line[len] = p[len];
            ++len;
        }
        line[len] = '\0';
        p += len;
        if (*p == '\n') ++p;

        if (line[0] == 'v' && line[1] == ' ') {
            RtsVec3f v = {0, 0, 0};
            if (sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3 &&
                grow_buf((void **)&pos, &pos_cap, sizeof(*pos), pos_n + 1)) {
                pos[pos_n++] = v;
            }
        } else if (line[0] == 'v' && line[1] == 't' && line[2] == ' ') {
            Vec2f t = {0, 0};
            if (sscanf(line + 3, "%f %f", &t.u, &t.v) == 2 &&
                grow_buf((void **)&uvs, &uv_cap, sizeof(*uvs), uv_n + 1)) {
                uvs[uv_n++] = t;
            }
        } else if (line[0] == 'v' && line[1] == 'n' && line[2] == ' ') {
            RtsVec3f n = {0, 1, 0};
            if (sscanf(line + 3, "%f %f %f", &n.x, &n.y, &n.z) == 3 &&
                grow_buf((void **)&nrm, &nrm_cap, sizeof(*nrm), nrm_n + 1)) {
                nrm[nrm_n++] = n;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            char toks[8][64];
            int count = 0;
            const char *q = line + 2;
            while (*q && count < 8) {
                while (*q == ' ' || *q == '\t' || *q == '\r') ++q;
                if (*q == '\0') break;
                int k = 0;
                while (*q && *q != ' ' && *q != '\t' && *q != '\r' && k < 63) {
                    toks[count][k++] = *q++;
                }
                toks[count][k] = '\0';
                count++;
            }
            for (int t = 1; t + 1 < count; ++t) {
                const char *tri_tok[3] = {toks[0], toks[t], toks[t + 1]};
                RtsVertex fv[3];
                bool has_normal = true;
                for (int k = 0; k < 3; ++k) {
                    int vi = 0, ti = 0, ni = 0;
                    parse_face_token(tri_tok[k], &vi, &ti, &ni);
                    RtsVec3f vp = (vi > 0 && (size_t)vi <= pos_n) ? pos[vi - 1] : (RtsVec3f){0, 0, 0};
                    Vec2f vt = (ti > 0 && (size_t)ti <= uv_n) ? uvs[ti - 1] : (Vec2f){(float)(k & 1), (float)(k >> 1)};
                    RtsVec3f vn = {0, 1, 0};
                    if (ni > 0 && (size_t)ni <= nrm_n) {
                        vn = nrm[ni - 1];
                    } else {
                        has_normal = false;
                    }
                    fv[k] = (RtsVertex){vp.x, vp.y, vp.z, vn.x, vn.y, vn.z, vt.u, vt.v, tint.x, tint.y, tint.z};
                }
                if (!has_normal) {
                    RtsVec3d e1 = rts_v3d(fv[1].px - fv[0].px, fv[1].py - fv[0].py, fv[1].pz - fv[0].pz);
                    RtsVec3d e2 = rts_v3d(fv[2].px - fv[0].px, fv[2].py - fv[0].py, fv[2].pz - fv[0].pz);
                    RtsVec3d fn = rts_v3d_norm(rts_v3d_cross(e1, e2));
                    for (int k = 0; k < 3; ++k) {
                        fv[k].nx = (float)fn.x;
                        fv[k].ny = (float)fn.y;
                        fv[k].nz = (float)fn.z;
                    }
                }
                if (grow_buf((void **)&out, &out_cap, sizeof(*out), out_n + 3)) {
                    out[out_n++] = fv[0];
                    out[out_n++] = fv[1];
                    out[out_n++] = fv[2];
                }
            }
        }
    }

    free(pos);
    free(uvs);
    free(nrm);

    if (out_n == 0) {
        free(out);
        return false;
    }
    out_mesh->verts = out;
    out_mesh->vert_count = out_n;
    return true;
}

void rts_obj_free(RtsObjMesh *mesh) {
    if (!mesh) return;
    free(mesh->verts);
    mesh->verts = NULL;
    mesh->vert_count = 0;
}
