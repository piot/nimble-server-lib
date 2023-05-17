/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <flood/in_stream.h>
#include <imprint/allocator.h>
#include <nimble-server/game.h>
#include <nimble-server/participant_connection.h>
#include <nimble-steps-serialize/out_serialize.h>

/// Initializes and allocated memory for a game.
/// A Game holds information about the latest game state for joining, authoritative steps and accepted participants.
/// @param self
/// @param allocator
/// @param maxSingleParticipantStepOctetCount
/// @param maxGameStateOctetSize
/// @param maxParticipantCount
/// @param log
void nimbleServerGameInit(NimbleServerGame* self, ImprintAllocator* allocator,
                          size_t maxSingleParticipantStepOctetCount, size_t maxGameStateOctetSize,
                          size_t maxParticipantCount, Clog log)
{
    self->log = log;
    size_t combinedStepOctetCount = nbsStepsOutSerializeCalculateCombinedSize(maxParticipantCount,
                                                                              maxSingleParticipantStepOctetCount);
    nbsStepsInit(&self->authoritativeSteps, allocator, combinedStepOctetCount, log);
    nbsStepsReInit(&self->authoritativeSteps, 0);
    nimbleServerGameStateInit(&self->latestState, allocator, maxGameStateOctetSize);
    nimbleServerParticipantsInit(&self->participants, allocator, maxParticipantCount);
}

static void nimbleServerGameShowReport(NimbleServerGame* game, NimbleServerParticipantConnections* connections)
{
    NbsSteps* steps = &game->authoritativeSteps;
    CLOG_C_INFO(&game->log, "Authoritative: steps %08X to %08X (count: %d) latestState: %08X", steps->expectedReadId,
                steps->expectedWriteId - 1, steps->stepsCount, game->latestState.stepId)

    for (size_t i = 0u; i < connections->connectionCount; ++i) {
        const NimbleServerParticipantConnection* connection = &connections->connections[i];
        if (connection->steps.stepsCount == 0) {
            CLOG_C_INFO(&game->log, " .. con#%d: no steps, lastStepId: %08X", connection->id,
                        connection->steps.expectedWriteId - 1)

        } else {
            CLOG_C_INFO(&game->log, " .. con#%d: steps: %08X to %08X count:%zu", connection->id,
                        connection->steps.expectedReadId, connection->steps.expectedWriteId - 1,
                        connection->steps.stepsCount)
        }
    }
    CLOG_C_INFO(&game->log, "Authoritative: -----")
}

int nimbleServerGameSetGameState(NimbleServerGame* game, StepId stepId, const uint8_t* gameState,
                                 size_t gameStateOctetCount, Clog* log)
{
    return nimbleServerGameStateSet(&game->latestState, stepId, gameState, gameStateOctetCount, log);
}
