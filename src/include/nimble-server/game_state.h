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

/// Holds the title-specific game state captured at @p NbdGameState::stepId.
typedef struct NbdGameState {
    uint8_t* state;
    size_t octetCount;
    size_t capacity;
    StepId stepId;
} NbdGameState;

void nbdGameStateInit(NbdGameState* self, struct ImprintAllocator* allocatorWithFree, size_t octetCount);
void nbdGameStateDestroy(NbdGameState* self);

#endif
