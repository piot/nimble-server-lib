/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <imprint/allocator.h>
#include <nimble-server/game_state.h>

/// Reserve space for a game state
/// @param self
/// @param allocator
/// @param octetCount
void nimbleServerGameStateInit(NimbleServerGameState* self, ImprintAllocator* allocator, size_t octetCount)
{
    self->state = IMPRINT_ALLOC_TYPE_COUNT(allocator, uint8_t, octetCount);
    self->capacity = octetCount;
    self->stepId = 0;
    self->octetCount = 0;
}

/// Should be set at given intervals to ensure the possibility of late-joining.
/// @param self
/// @param stepId
/// @param gameState
/// @param gameStateOctetCount
/// @param log
/// @return
int nimbleServerGameStateSet(NimbleServerGameState* state, StepId stepId, const uint8_t* gameState,
                                 size_t gameStateOctetCount, Clog* log)
{
    // CLOG_VERBOSE("trying to set game state %08X (octet count:%zu)", stepId, gameStateOctetCount);
    if (state->octetCount != 0 && stepId <= state->stepId) {
        CLOG_C_SOFT_ERROR(log, "ignoring old game state. we have %08X, but tried to set %08X", state->stepId, stepId);
        return 0;
    }
    int diff = stepId - state->stepId;
    if (diff < 5) {
        CLOG_C_NOTICE(log, "was set to a new state, but not that much newer %d", diff);
    }

    if (state->capacity < gameStateOctetCount) {
        CLOG_C_SOFT_ERROR(log, "can not set gamestate. Not enough capacity %zu vs %zu", gameStateOctetCount,
                          state->capacity);
        return -4;
    }

    tc_memcpy_octets(state->state, gameState, gameStateOctetCount);
    state->octetCount = gameStateOctetCount;
    state->stepId = stepId;

    CLOG_C_DEBUG(log, "Server now has a new state for stepId:%08X and octetCount:%zu", stepId, gameStateOctetCount);

    return 1;
}

