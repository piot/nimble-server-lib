/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_CONNECTION_QUALITY_H
#define NIMBLE_SERVER_CONNECTION_QUALITY_H

#include <imprint/tagged_allocator.h>
#include <nimble-steps/steps.h>
#include <stdbool.h>

typedef enum NimbleServerConnectionQualityDisconnectReason {
    NimbleServerConnectionQualityDisconnectReasonKeep,
    NimbleServerConnectionQualityDisconnectReasonNotProvidingStepsInTime,
} NimbleServerConnectionQualityDisconnectReason;

typedef struct NimbleServerConnectionQuality {
    size_t forcedStepInRowCounter;
    size_t providedStepsInARow;
    size_t addedStepsToBufferCounter;
    bool hasAddedFirstAcceptedSteps;

    char debugPrefix[64];
    Clog log;
} NimbleServerConnectionQuality;

void nimbleServerConnectionQualityInit(NimbleServerConnectionQuality* self, Clog log);
void nimbleServerConnectionQualityReset(NimbleServerConnectionQuality* self);
void nimbleServerConnectionQualityReInit(NimbleServerConnectionQuality* self);
void nimbleServerConnectionQualityProvidedUsableStep(NimbleServerConnectionQuality* self);
void nimbleServerConnectionQualityAddedStepsToBuffer(NimbleServerConnectionQuality* self, size_t count);
void nimbleServerConnectionQualityAddedForcedSteps(NimbleServerConnectionQuality* self, size_t count);
bool nimbleServerConnectionQualityCheckIfShouldDisconnect(const NimbleServerConnectionQuality* self);
const char* nimbleServerConnectionQualityDescribe(const NimbleServerConnectionQuality* self, char* buf, size_t maxBufSize);

#endif
