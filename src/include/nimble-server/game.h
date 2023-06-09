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
typedef struct NimbleServerGame {
    NbsSteps authoritativeSteps;
    NimbleServerGameState latestState;
    NimbleServerParticipants participants;
    Clog log;
} NimbleServerGame;

void nimbleServerGameInit(NimbleServerGame* self, struct ImprintAllocator* allocator,
                          size_t maxSingleParticipantStepOctetCount, size_t maxGameStateOctetSize,
                          size_t maxParticipantCount, Clog log);
int nimbleServerGameSetGameState(NimbleServerGame* game, StepId stepId, const uint8_t* gameState,
                                 size_t gameStateOctetCount, Clog* log);

#endif
