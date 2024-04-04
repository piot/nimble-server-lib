/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_SERIALIZED_GAME_STATE_H
#define NIMBLE_SERVER_SERIALIZED_GAME_STATE_H

#include <stddef.h>
#include <stdint.h>
#include <nimble-steps/steps.h>

typedef struct NimbleServerSerializedGameState {
    const uint8_t* gameState;
    size_t gameStateOctetCount;
    StepId stepId;
    uint64_t hash;
} NimbleServerSerializedGameState;

#endif
