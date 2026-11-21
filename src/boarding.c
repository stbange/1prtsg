/* SPDX-License-Identifier: MIT */
#include "rts/boarding.h"
#include <math.h>
#include <string.h>

void rts_boarding_init(RtsBoardingSystem *bs) {
    memset(bs, 0, sizeof(*bs));
    bs->next_op_id = 1u;
}

uint32_t rts_boarding_start(RtsBoardingSystem *bs, const RtsWorld *w, uint32_t target_entity_id, uint8_t attacker_faction, uint32_t marine_count, bool mag_boots) {
    if (!bs || !w || marine_count == 0u || bs->op_count >= RTS_MAX_BOARDING_OPS) {
        return 0u;
    }
    const RtsEntity *tgt = rts_world_get_entity_const(w, target_entity_id);
    if (!tgt || tgt->faction_id == attacker_faction) {
        return 0u;
    }
    RtsBoardingOp *op = &bs->ops[bs->op_count++];
    memset(op, 0, sizeof(*op));
    op->id = bs->next_op_id++;
    op->target_entity_id = target_entity_id;
    op->attacker_faction = attacker_faction;
    op->current_deck = RTS_DECK_HANGAR_AIRLOCK;
    op->status = RTS_BOARD_ACTIVE;
    op->attackers = marine_count;
    op->defenders = tgt->crew_count > 0u ? (tgt->crew_count / 2u + 2u) : 2u;
    op->attackers_mag_boots = mag_boots;
    op->active = true;
    return op->id;
}

bool rts_boarding_set_decompression(RtsBoardingSystem *bs, uint32_t op_id, bool decompressed) {
    if (!bs || op_id == 0u) return false;
    for (size_t i = 0; i < bs->op_count; ++i) {
        if (bs->ops[i].id == op_id && bs->ops[i].active) {
            bs->ops[i].decompressed = decompressed;
            return true;
        }
    }
    return false;
}

const RtsBoardingOp *rts_boarding_get_op(const RtsBoardingSystem *bs, uint32_t op_id) {
    if (!bs || op_id == 0u) return NULL;
    for (size_t i = 0; i < bs->op_count; ++i) {
        if (bs->ops[i].id == op_id) {
            return &bs->ops[i];
        }
    }
    return NULL;
}

bool rts_boarding_trigger_meltdown(RtsBoardingSystem *bs, RtsWorld *w, uint32_t entity_id, double countdown_s) {
    if (!bs || !w || countdown_s <= 0.0 || bs->meltdown_count >= RTS_MAX_MELTDOWNS) {
        return false;
    }
    RtsEntity *e = rts_world_get_entity(w, entity_id);
    if (!e) return false;
    /* Prevent duplicate meltdown on same entity */
    for (size_t i = 0; i < bs->meltdown_count; ++i) {
        if (bs->meltdowns[i].entity_id == entity_id && bs->meltdowns[i].active) {
            return false;
        }
    }
    RtsMeltdownEvent *ev = &bs->meltdowns[bs->meltdown_count++];
    ev->entity_id = entity_id;
    ev->countdown_s = countdown_s;
    ev->initial_s = countdown_s;
    ev->active = true;
    ev->detonated = false;
    e->subsystem_hp[RTS_SUBSYS_REACTOR] = 5.0f;
    return true;
}

const RtsMeltdownEvent *rts_boarding_get_meltdown(const RtsBoardingSystem *bs, uint32_t entity_id) {
    if (!bs || entity_id == 0u) return NULL;
    for (size_t i = 0; i < bs->meltdown_count; ++i) {
        if (bs->meltdowns[i].entity_id == entity_id) {
            return &bs->meltdowns[i];
        }
    }
    return NULL;
}

size_t rts_boarding_step(RtsBoardingSystem *bs, RtsWorld *w, double dt_s) {
    if (!bs || !w || dt_s <= 0.0) return 0u;
    size_t events_completed = 0u;

    /* 1. Step active Reactor Meltdowns */
    for (size_t i = 0; i < bs->meltdown_count; ++i) {
        RtsMeltdownEvent *ev = &bs->meltdowns[i];
        if (!ev->active || ev->detonated) continue;
        RtsEntity *e = rts_world_get_entity(w, ev->entity_id);
        if (!e) {
            ev->active = false;
            continue;
        }
        ev->countdown_s -= dt_s;
        if (ev->countdown_s <= 0.0) {
            ev->countdown_s = 0.0;
            ev->detonated = true;
            ev->active = false;
            /* Hardcore Ironman triggers automatically if Commander is still inside! */
            rts_world_destroy_entity(w, ev->entity_id);
            events_completed++;
        }
    }

    /* 2. Step active Multi-Deck Boarding Operations */
    for (size_t i = 0; i < bs->op_count; ++i) {
        RtsBoardingOp *op = &bs->ops[i];
        if (!op->active || op->status != RTS_BOARD_ACTIVE) continue;
        RtsEntity *tgt = rts_world_get_entity(w, op->target_entity_id);
        if (!tgt) {
            op->status = RTS_BOARD_ABORTED_DESTROYED;
            op->active = false;
            continue;
        }

        /* Deterministic fire-fight & vacuum decompression hazard */
        double att_power = (double)op->attackers * (op->decompressed && !op->attackers_mag_boots ? 0.45 : 1.0);
        double def_power = (double)op->defenders * (op->decompressed ? 0.60 : 1.0);

        if (op->defenders > 0u) {
            uint32_t def_loss = (uint32_t)fmax(1.0, floor(att_power * 0.25 * dt_s));
            uint32_t att_loss = (uint32_t)floor(def_power * 0.12 * dt_s);
            if (op->decompressed && !op->attackers_mag_boots) {
                att_loss += 1u;
            }
            op->defenders = (def_loss >= op->defenders) ? 0u : (op->defenders - def_loss);
            op->attackers = (att_loss >= op->attackers) ? 0u : (op->attackers - att_loss);
        }

        if (op->attackers == 0u) {
            op->status = RTS_BOARD_REPELLED;
            op->active = false;
            events_completed++;
            continue;
        }

        /* Advance deck breach progress once resistance weakens */
        double adv_rate = (op->defenders == 0u) ? 0.45 : 0.12;
        op->deck_progress += adv_rate * dt_s;
        if (op->deck_progress >= 1.0) {
            op->deck_progress = 0.0;
            if (op->current_deck < RTS_DECK_BRIDGE_CIC) {
                op->current_deck = (RtsBoardingDeck)(op->current_deck + 1);
                /* Spawn fresh interior security detail on next deck */
                op->defenders += 2u;
            } else {
                /* Bridge/CIC Secured -> Intact Capture! */
                tgt->faction_id = op->attacker_faction;
                tgt->crew_count = op->attackers;
                op->status = RTS_BOARD_CAPTURED;
                op->active = false;
                events_completed++;
            }
        }
    }

    return events_completed;
}
