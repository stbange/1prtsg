/* SPDX-License-Identifier: MIT */
#include "rts/math3d.h"
#include <math.h>
#include <string.h>

RtsVec3d rts_v3d(double x, double y, double z) {
    RtsVec3d r = {x, y, z};
    return r;
}

RtsVec3d rts_v3d_add(RtsVec3d a, RtsVec3d b) {
    return rts_v3d(a.x + b.x, a.y + b.y, a.z + b.z);
}

RtsVec3d rts_v3d_sub(RtsVec3d a, RtsVec3d b) {
    return rts_v3d(a.x - b.x, a.y - b.y, a.z - b.z);
}

RtsVec3d rts_v3d_scale(RtsVec3d v, double s) {
    return rts_v3d(v.x * s, v.y * s, v.z * s);
}

double rts_v3d_dot(RtsVec3d a, RtsVec3d b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

RtsVec3d rts_v3d_cross(RtsVec3d a, RtsVec3d b) {
    return rts_v3d(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

double rts_v3d_len_sq(RtsVec3d v) {
    return rts_v3d_dot(v, v);
}

double rts_v3d_len(RtsVec3d v) {
    return sqrt(rts_v3d_len_sq(v));
}

RtsVec3d rts_v3d_norm(RtsVec3d v) {
    double l = rts_v3d_len(v);
    if (l <= 1e-12) {
        return rts_v3d(0.0, 0.0, 0.0);
    }
    return rts_v3d_scale(v, 1.0 / l);
}

double rts_v3d_dist(RtsVec3d a, RtsVec3d b) {
    return rts_v3d_len(rts_v3d_sub(a, b));
}

RtsVec3f rts_v3d_to_f(RtsVec3d v) {
    RtsVec3f r = {(float)v.x, (float)v.y, (float)v.z};
    return r;
}

RtsVec3f rts_v3d_relative_f(RtsVec3d world_pos, RtsVec3d camera_origin) {
    return rts_v3d_to_f(rts_v3d_sub(world_pos, camera_origin));
}

RtsQuatd rts_quat_identity(void) {
    RtsQuatd q = {0.0, 0.0, 0.0, 1.0};
    return q;
}

RtsQuatd rts_quat_from_axis_angle(RtsVec3d axis, double radians) {
    RtsVec3d n = rts_v3d_norm(axis);
    if (rts_v3d_len_sq(n) <= 1e-12) {
        return rts_quat_identity();
    }
    double half = radians * 0.5;
    double s = sin(half);
    RtsQuatd q = {n.x * s, n.y * s, n.z * s, cos(half)};
    return q;
}

RtsQuatd rts_quat_norm(RtsQuatd q) {
    double l = sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (l <= 1e-12) {
        return rts_quat_identity();
    }
    double inv = 1.0 / l;
    RtsQuatd r = {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
    return r;
}

RtsQuatd rts_quat_conj(RtsQuatd q) {
    RtsQuatd r = {-q.x, -q.y, -q.z, q.w};
    return r;
}

RtsQuatd rts_quat_mul(RtsQuatd a, RtsQuatd b) {
    RtsQuatd r = {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
    return r;
}

RtsVec3d rts_quat_rotate(RtsQuatd q, RtsVec3d v) {
    RtsVec3d qv = rts_v3d(q.x, q.y, q.z);
    RtsVec3d t = rts_v3d_scale(rts_v3d_cross(qv, v), 2.0);
    RtsVec3d c = rts_v3d_cross(qv, t);
    return rts_v3d_add(rts_v3d_add(v, rts_v3d_scale(t, q.w)), c);
}

RtsMat4f rts_mat4_identity(void) {
    RtsMat4f r;
    memset(&r, 0, sizeof(r));
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

RtsMat4f rts_mat4_mul(RtsMat4f a, RtsMat4f b) {
    RtsMat4f r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return r;
}

RtsMat4f rts_mat4_translate(RtsVec3f t) {
    RtsMat4f r = rts_mat4_identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

RtsMat4f rts_mat4_scale(RtsVec3f s) {
    RtsMat4f r = rts_mat4_identity();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}

RtsMat4f rts_mat4_from_quat_trs(RtsQuatd rot, RtsVec3f pos, RtsVec3f scale) {
    RtsQuatd q = rts_quat_norm(rot);
    double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    RtsMat4f r = rts_mat4_identity();
    r.m[0] = (float)((1.0 - 2.0 * (yy + zz)) * scale.x);
    r.m[1] = (float)((2.0 * (xy + wz)) * scale.x);
    r.m[2] = (float)((2.0 * (xz - wy)) * scale.x);

    r.m[4] = (float)((2.0 * (xy - wz)) * scale.y);
    r.m[5] = (float)((1.0 - 2.0 * (xx + zz)) * scale.y);
    r.m[6] = (float)((2.0 * (yz + wx)) * scale.y);

    r.m[8] = (float)((2.0 * (xz + wy)) * scale.z);
    r.m[9] = (float)((2.0 * (yz - wx)) * scale.z);
    r.m[10] = (float)((1.0 - 2.0 * (xx + yy)) * scale.z);

    r.m[12] = pos.x;
    r.m[13] = pos.y;
    r.m[14] = pos.z;
    return r;
}

RtsMat4f rts_mat4_perspective(float fov_y_rad, float aspect, float near_z, float far_z) {
    RtsMat4f r;
    memset(&r, 0, sizeof(r));
    float f = 1.0f / tanf(fov_y_rad * 0.5f);
    float inv_nf = 1.0f / (near_z - far_z);
    r.m[0] = f / (aspect > 1e-6f ? aspect : 1.0f);
    r.m[5] = f;
    r.m[10] = (far_z + near_z) * inv_nf;
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * far_z * near_z) * inv_nf;
    return r;
}

RtsMat4f rts_mat4_ortho(float left, float right, float bottom, float top, float near_z, float far_z) {
    RtsMat4f r = rts_mat4_identity();
    r.m[0] = 2.0f / (right - left);
    r.m[5] = 2.0f / (top - bottom);
    r.m[10] = -2.0f / (far_z - near_z);
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    r.m[14] = -(far_z + near_z) / (far_z - near_z);
    return r;
}

RtsMat4f rts_mat4_look_at(RtsVec3f eye, RtsVec3f target, RtsVec3f up) {
    RtsVec3d f = rts_v3d_norm(rts_v3d(target.x - eye.x, target.y - eye.y, target.z - eye.z));
    RtsVec3d u = rts_v3d_norm(rts_v3d(up.x, up.y, up.z));
    RtsVec3d s = rts_v3d_norm(rts_v3d_cross(f, u));
    RtsVec3d u2 = rts_v3d_cross(s, f);

    RtsMat4f r = rts_mat4_identity();
    r.m[0] = (float)s.x;
    r.m[4] = (float)s.y;
    r.m[8] = (float)s.z;

    r.m[1] = (float)u2.x;
    r.m[5] = (float)u2.y;
    r.m[9] = (float)u2.z;

    r.m[2] = (float)(-f.x);
    r.m[6] = (float)(-f.y);
    r.m[10] = (float)(-f.z);

    RtsVec3d e = rts_v3d(eye.x, eye.y, eye.z);
    r.m[12] = (float)(-rts_v3d_dot(s, e));
    r.m[13] = (float)(-rts_v3d_dot(u2, e));
    r.m[14] = (float)(rts_v3d_dot(f, e));
    return r;
}

bool rts_aabb_contains(RtsAabb box, RtsVec3d p) {
    return p.x >= box.min.x && p.x <= box.max.x &&
           p.y >= box.min.y && p.y <= box.max.y &&
           p.z >= box.min.z && p.z <= box.max.z;
}

bool rts_ray_intersect_aabb(RtsRay ray, RtsAabb box, double *out_t) {
    double tmin = 0.0;
    double tmax = 1e30;
    double orig[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    double dir[3] = {ray.dir.x, ray.dir.y, ray.dir.z};
    double bmin[3] = {box.min.x, box.min.y, box.min.z};
    double bmax[3] = {box.max.x, box.max.y, box.max.z};

    for (int i = 0; i < 3; ++i) {
        if (fabs(dir[i]) < 1e-12) {
            if (orig[i] < bmin[i] || orig[i] > bmax[i]) {
                return false;
            }
        } else {
            double inv = 1.0 / dir[i];
            double t1 = (bmin[i] - orig[i]) * inv;
            double t2 = (bmax[i] - orig[i]) * inv;
            if (t1 > t2) {
                double tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) {
                return false;
            }
        }
    }
    if (out_t) {
        *out_t = tmin;
    }
    return true;
}

bool rts_ray_intersect_sphere(RtsRay ray, RtsVec3d center, double radius, double *out_t) {
    RtsVec3d d = rts_v3d_norm(ray.dir);
    if (rts_v3d_len_sq(d) <= 1e-12 || radius <= 0.0) {
        return false;
    }
    RtsVec3d oc = rts_v3d_sub(ray.origin, center);
    double b = rts_v3d_dot(oc, d);
    double c = rts_v3d_dot(oc, oc) - radius * radius;
    double disc = b * b - c;
    if (disc < 0.0) {
        return false;
    }
    double sq = sqrt(disc);
    double t = -b - sq;
    if (t < 0.0) {
        t = -b + sq;
    }
    if (t < 0.0) {
        return false;
    }
    if (out_t) {
        *out_t = t;
    }
    return true;
}

double rts_clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float rts_clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
