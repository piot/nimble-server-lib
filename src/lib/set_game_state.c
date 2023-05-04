/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <nimble-server/game.h>

/*
/// Discards all steps with StepId less than latestStateStepId.
/// @param self
/// @param latestStateStepId
/// @return
static int discardOldStepsBehindLatestState(NbdGame* self, StepId latestStateStepId)
{
    NbsSteps* authoritativeSteps = &self->authoritativeSteps;

    const int stepsToKeepBehindLatestState = 30;
    StepId oldestStepIdToKeep = 0;

    if (latestStateStepId > stepsToKeepBehindLatestState) {
        oldestStepIdToKeep = latestStateStepId - stepsToKeepBehindLatestState;
    }

    int countDiscarded = nbsStepsDiscardUpTo(authoritativeSteps, oldestStepIdToKeep);
    if (countDiscarded < 0) {
        CLOG_C_SOFT_ERROR(&self->log, "we couldn't discard up to %08X", oldestStepIdToKeep);
        return countDiscarded;
    }

    if (countDiscarded > 0) {
        CLOG_C_DEBUG(&self->log, "authoritative: discarded steps earlier than %04X (discarded %d)", oldestStepIdToKeep,
                     countDiscarded);
    }

    return countDiscarded;
}
*/

/// Should be set at given intervals to ensure the possibility of late-joining.
/// It also ensures that Steps older than stepId is discarded.
/// @param self
/// @param stepId
/// @param gameState
/// @param gameStateOctetCount
/// @return
int nbdGameSetGameState(NbdGame* self, StepId stepId, const uint8_t* gameState, size_t gameStateOctetCount)
{
    NbdGameState* state = &self->latestState;

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

//    discardOldStepsBehindLatestState(self, stepId);

    return 1;
}
