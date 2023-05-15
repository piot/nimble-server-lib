/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <imprint/allocator.h>
#include <nimble-server/game.h>
#include <nimble-steps-serialize/out_serialize.h>
#include <flood/in_stream.h>

/// Initializes and allocated memory for a game.
/// A Game holds information about the latest game state for joining, authoritative steps and accepted participants.
/// @param self
/// @param allocator
/// @param maxSingleParticipantStepOctetCount
/// @param maxGameStateOctetSize
/// @param maxParticipantCount
/// @param log
void nbdGameInit(NimbleServerGame* self, ImprintAllocator* allocator, size_t maxSingleParticipantStepOctetCount,
                 size_t maxGameStateOctetSize, size_t maxParticipantCount, Clog log)
{
    self->log = log;
    size_t combinedStepOctetCount = nbsStepsOutSerializeCalculateCombinedSize(maxParticipantCount,
                                                                              maxSingleParticipantStepOctetCount);
    nbsStepsInit(&self->authoritativeSteps, allocator, combinedStepOctetCount, log);
    nbsStepsReInit(&self->authoritativeSteps, 0);
    nbdGameStateInit(&self->latestState, allocator, maxGameStateOctetSize);
    nbdParticipantsInit(&self->participants, allocator, maxParticipantCount);
}

/// Should be set at given intervals to ensure the possibility of late-joining.
/// @param self
/// @param stepId
/// @param gameState
/// @param gameStateOctetCount
/// @return
int nbdGameSetGameState(NimbleServerGame* self, StepId stepId, const uint8_t* gameState, size_t gameStateOctetCount)
{
    NimbleServerGameState* state = &self->latestState;

    // CLOG_VERBOSE("trying to set game state %08X (octet count:%zu)", stepId, gameStateOctetCount);
    if (state->octetCount != 0 && stepId <= state->stepId) {
        CLOG_C_SOFT_ERROR(&self->log, "ignoring old game state. we have %08X, but tried to set %08X", state->stepId,
                          stepId);
        return 0;
    }
    int diff = stepId - state->stepId;
    if (diff < 5) {
        CLOG_C_NOTICE(&self->log, "was set to a new state, but not that much newer %d", diff);
    }

    if (state->capacity < gameStateOctetCount) {
        CLOG_C_SOFT_ERROR(&self->log, "can not set gamestate. Not enough capacity %zu vs %zu", gameStateOctetCount,
                          state->capacity);
        return -4;
    }

    tc_memcpy_octets(state->state, gameState, gameStateOctetCount);
    state->octetCount = gameStateOctetCount;
    state->stepId = stepId;

    CLOG_C_DEBUG(&self->log, "Server now has a new state for stepId:%08X and octetCount:%zu", stepId,
                 gameStateOctetCount);

    return 1;
}


/// Destroy the game
/// @param self
void nbdGameDestroy(NimbleServerGame* self)
{
    nbdGameStateDestroy(&self->latestState);
}
