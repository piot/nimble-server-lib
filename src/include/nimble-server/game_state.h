/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_GAME_STATE_H
#define NIMBLE_SERVER_GAME_STATE_H

#include <nimble-steps/steps.h>
#include <stddef.h>
#include <stdint.h>

struct ImprintAllocator;

/// Holds the title-specific game state captured at @p NimbleServerGameState::stepId.
typedef struct NimbleServerGameState {
    uint8_t* state;
    size_t octetCount;
    size_t capacity;
    StepId stepId;
} NimbleServerGameState;

void nimbleServerGameStateInit(NimbleServerGameState* self, struct ImprintAllocator* allocatorWithFree,
                               size_t octetCount);
int nimbleServerGameStateSet(NimbleServerGameState* state, StepId stepId, const uint8_t* gameState,
                             size_t gameStateOctetCount, Clog* log);

#endif
