/* SPDX-License-Identifier: MIT */
#ifndef RTS_SAVE_LOAD_H
#define RTS_SAVE_LOAD_H

#include "rts/economy.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>

/* Deterministic text snapshot serializer & deserializer (RTS_SAVE_V1) */
size_t rts_save_serialize(const RtsWorld *w, const RtsEconomySystem *eco, char *out_buf, size_t buf_cap);
bool rts_save_deserialize(const char *snapshot_text, RtsWorld *w, RtsEconomySystem *eco);

/* Convenience OS file round-trip helpers */
bool rts_save_to_file(const char *os_path, const RtsWorld *w, const RtsEconomySystem *eco);
bool rts_load_from_file(const char *os_path, RtsWorld *w, RtsEconomySystem *eco);

#endif /* RTS_SAVE_LOAD_H */
