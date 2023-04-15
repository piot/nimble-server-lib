/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-server/game.h>
#include <imprint/allocator.h>

void nbdGameInit(NbdGame *self, ImprintAllocator *allocator, size_t maxCount) {
    nbsStepsInit(&self->authoritativeSteps, allocator, maxCount);
    nbsStepsReInit(&self->authoritativeSteps, 0);
#define MAX_GAMESTATE_SIZE (64 * 1024)
    nbdGameStateInit(&self->latestState, allocator, MAX_GAMESTATE_SIZE);
#define MAX_PLAYERS_IN_SESSION (32)
    nbdParticipantsInit(&self->participants, allocator, MAX_PLAYERS_IN_SESSION);
}

void nbdGameDestroy(NbdGame *self) {
    nbdGameStateDestroy(&self->latestState);
    nbdParticipantsDestroy(&self->participants);
}
