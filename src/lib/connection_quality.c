#include <nimble-server/connection_quality.h>

void nimbleServerConnectionQualityInit(NimbleServerConnectionQuality* self, size_t participantConnectionId, Clog log)
{
    nimbleServerConnectionQualityReset(self);
    self->log = log;
    self->participantConnectionId = participantConnectionId;
}

void nimbleServerConnectionQualityReset(NimbleServerConnectionQuality* self)
{
    self->providedStepsInARow = 0;
    self->forcedStepInRowCounter = 0;
    self->impedingDisconnectCounter = 0;
    self->hasAddedFirstAcceptedSteps = false;
}

void nimbleServerConnectionQualityReInit(NimbleServerConnectionQuality* self)
{
    nimbleServerConnectionQualityReset(self);
}

static bool isFailingToProvideStepsInTime(const NimbleServerConnectionQuality* self)
{
    size_t forcedStepInRowCounterThreshold = 8U;
    if (!self->hasAddedFirstAcceptedSteps) {
        forcedStepInRowCounterThreshold = 180U;
    }

    return self->forcedStepInRowCounter >= forcedStepInRowCounterThreshold;
}

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

void nimbleServerConnectionQualityAddedStepsToBuffer(NimbleServerConnectionQuality* self, size_t count)
{
    self->addedStepsToBufferCounter += count;
}

void nimbleServerConnectionQualityAddedForcedSteps(NimbleServerConnectionQuality* self, size_t count)
{
    self->providedStepsInARow = 0;
    self->forcedStepInRowCounter += count;

}

const char* nimbleServerConnectionQualityDescribe(NimbleServerConnectionQuality* self, char* buf, size_t maxBufSize)
{
    NimbleServerConnectionQualityDisconnectReason reason = evaluate(self);
    switch (reason) {
        case NimbleServerConnectionQualityDisconnectReasonKeep:
            tc_snprintf(buf, maxBufSize, "it is all good!");
            break;
        case NimbleServerConnectionQualityDisconnectReasonNotProvidingStepsInTime:
            tc_snprintf(buf, maxBufSize,
                        "not receiving steps from client in time. %zu forced steps in a row",
                        self->forcedStepInRowCounter);
            break;
    }

    return buf;
}

bool
nimbleServerConnectionQualityCheckIfShouldDisconnect(NimbleServerConnectionQuality* self)
{
    NimbleServerConnectionQualityDisconnectReason reason = evaluate(self);
    if (reason != NimbleServerConnectionQualityDisconnectReasonKeep) {
        self->impedingDisconnectCounter++;
        if (self->impedingDisconnectCounter > 120) {
                        #define BUF_SIZE (128)
            char buf[BUF_SIZE];
            CLOG_C_NOTICE(&self->log, "giving up on connection %zu, disconnecting %s",
                          self->participantConnectionId, nimbleServerConnectionQualityDescribe(self, buf, BUF_SIZE))
                        self->reason = reason;
            return true;
        }

        if ((self->impedingDisconnectCounter % 20) == 0) {
            #define BUF_SIZE (128)
            char buf[BUF_SIZE];
            CLOG_C_DEBUG(&self->log, "quality is really bad for connection %zu, considering disconnecting %s",
                         self->participantConnectionId, nimbleServerConnectionQualityDescribe(self, buf, BUF_SIZE))
        }
    } else {
        if (self->impedingDisconnectCounter > 0) {
            self->impedingDisconnectCounter--;
            if (self->impedingDisconnectCounter == 0) {
                CLOG_C_DEBUG(&self->log, "connection %zu has stabilized again", self->participantConnectionId)
            }
        }
    }

    return false;
}
