/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_GAME_H
#define NIMBLE_SERVER_GAME_H

#include <nimble-server/game_state.h>
#include <nimble-server/participant_connections.h>
#include <nimble-steps/steps.h>

struct ImprintAllocator;

/// Tracks the latestState, as well as the all authoritative Steps after the game state.
typedef struct NbdGame {
    NbsSteps authoritativeSteps;
    NbdGameState latestState;
    NbdParticipants participants;
} NbdGame;

void nbdGameInit(NbdGame* self, struct ImprintAllocator* allocator, size_t maxCount);
void nbdGameDestroy(NbdGame* self);
int nbdGameSetGameState(NbdGame* game, StepId stepId, const uint8_t* gameState, size_t gameStateOctetCount);


#endif
