/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_GAME_H
#define NIMBLE_SERVER_GAME_H

#include <nimble-server/game_state.h>
#include <nimble-server/local_parties.h>
#include <nimble-steps/steps.h>
#include <stdbool.h>

struct ImprintAllocator;

/// Tracks the latestState, as well as the all authoritative Steps after the game state.
typedef struct NimbleServerGame {
    NbsSteps authoritativeSteps;
    NimbleServerParticipants participants;
    bool debugIsFrozen;
    Clog log;
} NimbleServerGame;

void nimbleServerGameInit(NimbleServerGame* self, struct ImprintAllocator* allocator,
                          size_t maxSingleParticipantStepOctetCount, size_t maxParticipantCount, Clog log);


#endif
