/* SPDX-License-Identifier: MIT */
#ifndef RTS_MATH3D_H
#define RTS_MATH3D_H

#include <stdbool.h>
#include <stddef.h>

#define RTS_PI 3.14159265358979323846
#define RTS_DEG2RAD (RTS_PI / 180.0)
#define RTS_RAD2DEG (180.0 / RTS_PI)

typedef struct RtsVec3d {
    double x;
    double y;
    double z;
} RtsVec3d;

typedef struct RtsVec3f {
    float x;
    float y;
    float z;
} RtsVec3f;

typedef struct RtsVec4f {
    float x;
    float y;
    float z;
    float w;
} RtsVec4f;

typedef struct RtsQuatd {
    double x;
    double y;
    double z;
    double w;
} RtsQuatd;

/* Column-major 4x4 float matrix (OpenGL / GLSL convention). */
typedef struct RtsMat4f {
    float m[16];
} RtsMat4f;

typedef struct RtsAabb {
    RtsVec3d min;
    RtsVec3d max;
} RtsAabb;

typedef struct RtsRay {
    RtsVec3d origin;
    RtsVec3d dir;
} RtsRay;

RtsVec3d rts_v3d(double x, double y, double z);
RtsVec3d rts_v3d_add(RtsVec3d a, RtsVec3d b);
RtsVec3d rts_v3d_sub(RtsVec3d a, RtsVec3d b);
RtsVec3d rts_v3d_scale(RtsVec3d v, double s);
double rts_v3d_dot(RtsVec3d a, RtsVec3d b);
RtsVec3d rts_v3d_cross(RtsVec3d a, RtsVec3d b);
double rts_v3d_len_sq(RtsVec3d v);
double rts_v3d_len(RtsVec3d v);
RtsVec3d rts_v3d_norm(RtsVec3d v);
double rts_v3d_dist(RtsVec3d a, RtsVec3d b);
RtsVec3f rts_v3d_to_f(RtsVec3d v);
RtsVec3f rts_v3d_relative_f(RtsVec3d world_pos, RtsVec3d camera_origin);

RtsQuatd rts_quat_identity(void);
RtsQuatd rts_quat_from_axis_angle(RtsVec3d axis, double radians);
RtsQuatd rts_quat_norm(RtsQuatd q);
RtsQuatd rts_quat_conj(RtsQuatd q);
RtsQuatd rts_quat_mul(RtsQuatd a, RtsQuatd b);
RtsVec3d rts_quat_rotate(RtsQuatd q, RtsVec3d v);

RtsMat4f rts_mat4_identity(void);
RtsMat4f rts_mat4_mul(RtsMat4f a, RtsMat4f b);
RtsMat4f rts_mat4_translate(RtsVec3f t);
RtsMat4f rts_mat4_scale(RtsVec3f s);
RtsMat4f rts_mat4_from_quat_trs(RtsQuatd rot, RtsVec3f pos, RtsVec3f scale);
RtsMat4f rts_mat4_perspective(float fov_y_rad, float aspect, float near_z, float far_z);
RtsMat4f rts_mat4_ortho(float left, float right, float bottom, float top, float near_z, float far_z);
RtsMat4f rts_mat4_look_at(RtsVec3f eye, RtsVec3f target, RtsVec3f up);

bool rts_aabb_contains(RtsAabb box, RtsVec3d p);
bool rts_ray_intersect_aabb(RtsRay ray, RtsAabb box, double *out_t);
bool rts_ray_intersect_sphere(RtsRay ray, RtsVec3d center, double radius, double *out_t);
double rts_clampd(double v, double lo, double hi);
float rts_clampf(float v, float lo, float hi);

#endif /* RTS_MATH3D_H */
