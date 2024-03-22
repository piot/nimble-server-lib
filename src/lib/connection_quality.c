/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-server/connection_quality.h>

/// Initialize the connection quality
/// @param self connection quality
/// @param log logging
void nimbleServerConnectionQualityInit(NimbleServerConnectionQuality* self, Clog log)
{
    self->log = log;
    nimbleServerConnectionQualityReset(self);
}

/// Clear all values, except participant connection id and log
/// @param self connection quality
void nimbleServerConnectionQualityReset(NimbleServerConnectionQuality* self)
{
    self->providedStepsInARow = 0;
    self->forcedStepInRowCounter = 0;
    self->hasAddedFirstAcceptedSteps = false;
    self->addedStepsToBufferCounter = 0;
}

/// Reuse the existing connection quality
/// @param self connection quality
void nimbleServerConnectionQualityReInit(NimbleServerConnectionQuality* self)
{
    nimbleServerConnectionQualityReset(self);
}

/// check if there are many ticks where steps have not been provided in time
/// @param self connection quality
/// @return false if there are a lot of forced steps
static bool isFailingToProvideStepsInTime(const NimbleServerConnectionQuality* self)
{
    size_t forcedStepInRowCounterThreshold = 20U;
    if (!self->hasAddedFirstAcceptedSteps) {
        forcedStepInRowCounterThreshold = 200U;
    }

    return self->forcedStepInRowCounter >= forcedStepInRowCounterThreshold;
}

/// Evaluate the connection quality right now
/// @param self connection quality
/// @return the disconnect decision
/// @retval NimbleServerConnectionQualityDisconnectReasonNotProvidingStepsInTime if steps are not provided in time
/// @retval NimbleServerConnectionQualityDisconnectReasonKeep connection should be kept
static NimbleServerConnectionQualityDisconnectReason evaluate(const NimbleServerConnectionQuality* self)
{
    if (isFailingToProvideStepsInTime(self)) {
        return NimbleServerConnectionQualityDisconnectReasonNotProvidingStepsInTime;
    }

    return NimbleServerConnectionQualityDisconnectReasonKeep;
}

/// A step has been accepted as part of an authoritative step
/// @param self connection quality
void nimbleServerConnectionQualityProvidedUsableStep(NimbleServerConnectionQuality* self)
{
    self->forcedStepInRowCounter = 0;
    self->providedStepsInARow++;
    if (!self->hasAddedFirstAcceptedSteps) {
        self->hasAddedFirstAcceptedSteps = true;
        self->forcedStepInRowCounter = 0;
    }
    self->addedStepsToBufferCounter = 0;
}

/// Steps are accepted into the incoming steps buffer
/// @param self connection quality
/// @param count number of steps added to the buffer
void nimbleServerConnectionQualityAddedStepsToBuffer(NimbleServerConnectionQuality* self, size_t count)
{
    self->addedStepsToBufferCounter += count;
}

/// Forced steps has been added when computing an authoritative step
/// @param self connection quality
/// @param count number of steps that has been forced
void nimbleServerConnectionQualityAddedForcedSteps(NimbleServerConnectionQuality* self, size_t count)
{
    self->providedStepsInARow = 0;
    self->forcedStepInRowCounter += count;
}

/// Returns a human readable description of the disconnect decision
///@param self connection quality
///@param buf buffer to store the string
///@param maxBufSize maximum number of characters in the string
///@return the filled out buf
const char* nimbleServerConnectionQualityDescribe(const NimbleServerConnectionQuality* self, char* buf, size_t maxBufSize)
{
    NimbleServerConnectionQualityDisconnectReason reason = evaluate(self);
    switch (reason) {
        case NimbleServerConnectionQualityDisconnectReasonKeep:
            tc_snprintf(buf, maxBufSize, "connection should be kept");
            break;
        case NimbleServerConnectionQualityDisconnectReasonNotProvidingStepsInTime:
            tc_snprintf(buf, maxBufSize, "not receiving steps from client in time. %zu forced steps in a row",
                        self->forcedStepInRowCounter);
            break;
    }

    return buf;
}

/// checks if the connection has been decided to be disconnected
/// @param self connection quality
/// @return true if the connection should be disconnected
bool nimbleServerConnectionQualityCheckIfShouldDisconnect(const NimbleServerConnectionQuality* self)
{
    NimbleServerConnectionQualityDisconnectReason reason = evaluate(self);
    return reason != NimbleServerConnectionQualityDisconnectReasonKeep;
}
