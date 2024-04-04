/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <imprint/allocator.h>
#include <nimble-server/game_state.h>

/// Reserve space for a game state
/// @param self game state
/// @param allocator allocator to use
/// @param octetCount game state capacity in octet count
void nimbleServerGameStateInit(NimbleServerGameState* self, ImprintAllocator* allocator, size_t octetCount)
{
    self->state = IMPRINT_ALLOC_TYPE_COUNT(allocator, uint8_t, octetCount);
    self->capacity = octetCount;
    self->stepId = 0;
    self->octetCount = 0;
}

/// Copies a serialized game state
/// @param state game state
/// @param stepId stepID for the received game state
/// @param gameState application specific game state
/// @param gameStateOctetCount octet size of gameState
/// @param log target log
/// @return negative on error
int nimbleServerGameStateSet(NimbleServerGameState* state, StepId stepId, const uint8_t* gameState,
                             size_t gameStateOctetCount, Clog* log)
{
    (void) log;

    // CLOG_VERBOSE("trying to set game state %08X (octet count:%zu)", stepId, gameStateOctetCount);
    if (state->octetCount != 0 && stepId <= state->stepId) {
        CLOG_C_SOFT_ERROR(log, "ignoring old game state. we have %08X, but tried to set %08X", state->stepId, stepId)
        return 0;
    }
    size_t diff = stepId - state->stepId;
    if (diff < 5) {
        CLOG_C_NOTICE(log, "was set to a new state, but not that much newer %zu", diff)
    }

    if (state->capacity < gameStateOctetCount) {
        CLOG_C_SOFT_ERROR(log, "can not set gamestate. Not enough capacity %zu vs %zu", gameStateOctetCount,
                          state->capacity)
        return -4;
    }

    tc_memcpy_octets(state->state, gameState, gameStateOctetCount);
    state->octetCount = gameStateOctetCount;
    state->stepId = stepId;

    CLOG_C_DEBUG(log, "A GameState set to stepId:%08X and octetCount:%zu", stepId, gameStateOctetCount)

    return 1;
}

/// Copy game state
/// @param state game state
/// @param copyFrom application specific game state to copy from
/// @param log target log
/// @return negative on error
int nimbleServerGameStateCopy(NimbleServerGameState* state, const NimbleServerGameState* copyFrom, Clog* log)
{
    return nimbleServerGameStateSet(state, copyFrom->stepId, copyFrom->state, copyFrom->octetCount, log);
}
