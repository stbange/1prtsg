/* SPDX-License-Identifier: MIT */
#include "rts/planet.h"
#include "unity.h"
#include <math.h>

static void test_cube_sphere_and_enu_frames(void) {
    RtsPlanetConfig p = rts_planet_earthlike(42u);
    for (int f = 0; f < 6; ++f) {
        RtsVec3d u = rts_planet_cube_to_sphere((RtsCubeFace)f, 0.35, -0.42);
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, rts_v3d_len(u));
    }

    /* Exercise ENU frames across quadrants & pole to cover all basis branches */
    double lats[5] = {0.0, 0.6, -0.6, RTS_PI * 0.5, -RTS_PI * 0.5};
    double lons[5] = {0.0, 1.2, 2.5, 4.0, 5.5};
    for (int i = 0; i < 5; ++i) {
        RtsEnuFrame enu = rts_planet_enu_frame(&p, lats[i], lons[i], 10.0);
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, rts_v3d_len(enu.up));
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, rts_v3d_dot(enu.east, enu.up));
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, rts_v3d_dot(enu.north, enu.up));
        TEST_ASSERT_TRUE(rts_planet_altitude(&p, enu.origin) >= 9.9);
    }

    /* Check ocean vs land detection across globe */
    bool saw_ocean = false, saw_land = false;
    for (int i = 0; i < 20; ++i) {
        RtsVec3d u = rts_planet_latlon_to_unit((i - 10) * 0.14, i * 0.45);
        if (rts_planet_is_ocean(&p, u)) saw_ocean = true; else saw_land = true;
    }
    TEST_ASSERT_TRUE(saw_ocean && saw_land);
}

static void test_atmosphere_and_keplerian_orbits(void) {
    RtsPlanetConfig p = rts_planet_earthlike(7u);
    double rho0 = rts_planet_atmo_density(&p, 0.0);
    double rho_sub = rts_planet_atmo_density(&p, -100.0);
    double rho40 = rts_planet_atmo_density(&p, 40000.0);
    double rho_vac = rts_planet_atmo_density(&p, 250000.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, rho0, rho_sub);
    TEST_ASSERT_TRUE(rho40 > 0.0 && rho40 < rho0 * 0.02);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, rho_vac);

    TEST_ASSERT_TRUE(rts_planet_reentry_heat_flux(&p, 50000.0, 6500.0) > 10.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, rts_planet_reentry_heat_flux(&p, 250000.0, 7500.0));

    /* Gravity zero-distance guard */
    RtsVec3d g0 = rts_planet_gravity_accel(&p, rts_v3d(0, 0, 0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, rts_v3d_len(g0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, rts_orbit_circular_speed(&p, -p.radius_m));

    /* 400 km inclined circular orbit */
    RtsOrbitState st = rts_orbit_circular_state(&p, 400000.0, 0.4, 0.3);
    RtsOrbitElements el = rts_orbit_from_state(&p, st);
    TEST_ASSERT_TRUE(el.is_bound);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 0.0, el.eccentricity);
    TEST_ASSERT_DOUBLE_WITHIN(1500.0, 400000.0, el.periapsis_alt_m);
    TEST_ASSERT_TRUE(el.period_s > 5400.0 && el.period_s < 5700.0);

    /* Propagate 60 seconds with zero thrust/drag -> stable altitude */
    for (int i = 0; i < 60; ++i) {
        st = rts_orbit_propagate_step(&p, st, rts_v3d(0, 0, 0), 0.0, 1.0);
    }
    TEST_ASSERT_DOUBLE_WITHIN(500.0, 400000.0, rts_planet_altitude(&p, st.pos_m));

    /* Low-altitude elliptical + drag + escape trajectory branches */
    RtsOrbitState ell = rts_orbit_circular_state(&p, 50000.0, 0.6, 2.4);
    ell.vel_mps = rts_v3d_scale(ell.vel_mps, 1.15);
    RtsOrbitElements el_ell = rts_orbit_from_state(&p, ell);
    TEST_ASSERT_TRUE(el_ell.is_bound && el_ell.eccentricity > 0.2);
    RtsOrbitState dragged = rts_orbit_propagate_step(&p, ell, rts_v3d(0, 0, 0), 0.01, 1.0);
    TEST_ASSERT_TRUE(rts_v3d_len(dragged.vel_mps) < rts_v3d_len(ell.vel_mps));

    /* Negative z/y descending eccentric branch */
    RtsOrbitState ell2 = {rts_v3d(7000000.0, -500000.0, -1000000.0), rts_v3d(-1500.0, -2500.0, 8200.0)};
    RtsOrbitElements el2 = rts_orbit_from_state(&p, ell2);
    TEST_ASSERT_TRUE(el2.eccentricity > 0.0);

    /* Escape hyperbolic trajectory & zero-state guard */
    RtsOrbitState esc = {rts_v3d(7000000.0, 0.0, 0.0), rts_v3d(0.0, 0.0, 15000.0)};
    RtsOrbitElements el_esc = rts_orbit_from_state(&p, esc);
    TEST_ASSERT_FALSE(el_esc.is_bound);
    RtsOrbitElements el_bad = rts_orbit_from_state(&p, (RtsOrbitState){rts_v3d(0,0,0), rts_v3d(0,0,0)});
    TEST_ASSERT_FALSE(el_bad.is_bound);

    /* Sample orbit points + invalid sample args */
    RtsVec3d pts[16];
    TEST_ASSERT_EQUAL_UINT(16, rts_orbit_sample_points(&p, st, pts, 16));
    TEST_ASSERT_EQUAL_UINT(0, rts_orbit_sample_points(&p, st, NULL, 16));

    /* Hohmann transfer LEO -> GEO + invalid guard */
    RtsHohmannTransfer h = rts_orbit_hohmann(&p, 400000.0, 35786000.0);
    TEST_ASSERT_TRUE(h.valid && h.total_dv_mps > 3500.0 && h.transfer_time_s > 18000.0);
    RtsHohmannTransfer hbad = rts_orbit_hohmann(&p, -p.radius_m, 400000.0);
    TEST_ASSERT_FALSE(hbad.valid);
}

void register_planet_tests(void) {
    RUN_TEST(test_cube_sphere_and_enu_frames);
    RUN_TEST(test_atmosphere_and_keplerian_orbits);
}
