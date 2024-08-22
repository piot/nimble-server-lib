/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_PARTICIPANT_H
#define NIMBLE_SERVER_PARTICIPANT_H

#include <nimble-steps/steps.h>
#include <stdbool.h>
#include <stdlib.h>

struct NimbleServerLocalParty;
struct ImprintAllocator;
struct FldInStream;

typedef enum NimbleServerParticipantState {
    NimbleServerParticipantStateJustJoined,
    NimbleServerParticipantStateNormal,
    NimbleServerParticipantStateWaitingForRejoin,
    NimbleServerParticipantStateLeaving,
    NimbleServerParticipantStateDestroyed,
} NimbleServerParticipantState;

typedef struct NimbleServerParticipant {
    size_t localIndex;
    uint8_t id;
    bool isUsed;
    NbsSteps steps;

    struct NimbleServerLocalParty* inParty;
    NimbleServerParticipantState state;
    Clog log;
    char debugPrefix[32];
} NimbleServerParticipant;

typedef struct NimbleServerParticipantSetup {
    uint8_t id;
    struct ImprintAllocator* connectionAllocator;
    size_t maxStepOctetSizeForOneParticipant;
    Clog log;
} NimbleServerParticipantSetup;

void nimbleServerParticipantInit(NimbleServerParticipant* self, NimbleServerParticipantSetup setup);
void nimbleServerParticipantReInit(NimbleServerParticipant* self, struct NimbleServerLocalParty* party, StepId stepId);
void nimbleServerParticipantDestroy(NimbleServerParticipant* self);
void nimbleServerParticipantMarkAsLeaving(NimbleServerParticipant* self);
int nimbleServerParticipantDeserializeSingleStep(NimbleServerParticipant* self, StepId stepId,
                                                 struct FldInStream* inStream);

#endif
