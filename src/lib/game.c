/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <imprint/allocator.h>
#include <nimble-server/game.h>
#include <nimble-steps-serialize/out_serialize.h>

void nbdGameInit(NbdGame* self, ImprintAllocator* allocator, size_t maxSingleParticipantStepOctetCount,
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

void nbdGameDestroy(NbdGame* self)
{
    nbdGameStateDestroy(&self->latestState);
    nbdParticipantsDestroy(&self->participants);
}
