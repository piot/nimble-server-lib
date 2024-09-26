/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#include <imprint/allocator.h>
#include <nimble-server/game.h>
#include <nimble-steps-serialize/out_serialize.h>

/// Initializes and allocated memory for a game.
/// A Game holds information about the latest game state for joining, authoritative steps and accepted participants.
/// @param self game
/// @param allocator allocator
/// @param maxSingleParticipantStepOctetCount maximum octet count for a single participant
/// @param maxParticipantCount maximum number of participants in a game
/// @param log target log
void nimbleServerGameInit(NimbleServerGame* self, ImprintAllocator* allocator,
                          size_t maxSingleParticipantStepOctetCount, size_t maxParticipantCount, Clog log)
{
    self->log = log;
    self->debugIsFrozen = false;
    size_t combinedStepOctetCount = nbsStepsOutSerializeCalculateCombinedSize(maxParticipantCount,
                                                                              maxSingleParticipantStepOctetCount);
    nbsStepsInit(&self->authoritativeSteps, allocator, combinedStepOctetCount, log);
    nbsStepsReInit(&self->authoritativeSteps, 0);
    tc_snprintf(self->participants.debugPrefix, sizeof(self->participants.debugPrefix), "%s/participants",
                self->log.constantPrefix);

    nimbleServerParticipantsInit(&self->participants, allocator, maxParticipantCount,
                                 maxSingleParticipantStepOctetCount, &self->log);
}

#if 0
static void nimbleServerGameShowReport(NimbleServerGame* game, NimbleServerLocalParties* connections)
{
    NbsSteps* steps = &game->authoritativeSteps;
    CLOG_C_INFO(&game->log, "Authoritative: steps %08X to %08X (count: %zu) latestState: %08X", steps->expectedReadId,
                steps->expectedWriteId - 1, steps->stepsCount, game->latestState.stepId)

    for (size_t i = 0u; i < connections->connectionCount; ++i) {
        const NimbleServerLocalParty* connection = &connections->connections[i];
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
#endif
