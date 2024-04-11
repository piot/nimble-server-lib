/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include "authoritative_steps.h"
#include "incoming_predicted_steps.h"
#include "nimble-server/server.h"
#include <flood/in_stream.h>
#include <nimble-server/delayed_quality.h>
#include <nimble-server/forced_step.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <nimble-steps-serialize/out_serialize.h>

/// Initializes a participant connection.
/// @param self the participant connection
/// @param connectionAllocator allocator for connection collection
/// @param currentAuthoritativeStepId latest authoritative state step ID
/// @param maxParticipantCountForConnection maximum number of participants for a single connection (e.g. for
/// split-screen it is usually 2-4).
/// @param transportConnection the transport connection id
/// @param maxSingleParticipantStepOctetCount max number of octets for one single step.
/// @param log the log to use for logging
/// Need to create Participants to the game before associating them to the connection.
void nimbleServerParticipantConnectionInit(NimbleServerParticipantConnection* self,
                                           NimbleServerTransportConnection* transportConnection,
                                           ImprintAllocator* connectionAllocator, StepId currentAuthoritativeStepId,
                                           size_t maxParticipantCountForConnection,
                                           size_t maxSingleParticipantStepOctetCount, Clog log)
{
    size_t combinedStepOctetSize = nbsStepsOutSerializeCalculateCombinedSize(maxParticipantCountForConnection,
                                                                             maxSingleParticipantStepOctetCount);

    self->log = log;
    nbsStepsInit(&self->steps, connectionAllocator, combinedStepOctetSize, log);
    self->participantReferences.participantReferenceCount = 0;
    self->waitingForReconnectMaxTimer = 62 * 20;
    self->isUsed = false;
    self->warningCount = 0;

    self->quality.log.config = log.config;
    tc_snprintf(self->quality.debugPrefix, sizeof(self->quality.debugPrefix), "%s/quality", self->log.constantPrefix);
    self->quality.log.constantPrefix = self->quality.debugPrefix;
    nimbleServerConnectionQualityInit(&self->quality, self->quality.log);
    nimbleServerConnectionQualityDelayedInit(&self->delayedQuality, self->quality.log);

    CLOG_C_DEBUG(&self->log, "initialize new participant connection")
    nimbleServerParticipantConnectionReInit(self, transportConnection, currentAuthoritativeStepId);
}

/// Resets the participant connection
/// @param self participant connection
void nimbleServerParticipantConnectionReset(NimbleServerParticipantConnection* self)
{
    self->isUsed = false;
    self->participantReferences.participantReferenceCount = 0;
    self->waitingForReconnectTimer = 0;
    self->warningCount = 0;
    self->warningAboutZeroAddedSteps = 0;
    nimbleServerConnectionQualityReset(&self->quality);
    nimbleServerConnectionQualityDelayedReset(&self->delayedQuality);
}

/// Reinitialize the participant connection
/// @param self participant connection
/// @param transportConnection the underlying transport connection
/// @param currentAuthoritativeStepId the authoritative step ID to start from
void nimbleServerParticipantConnectionReInit(NimbleServerParticipantConnection* self,
                                             NimbleServerTransportConnection* transportConnection,
                                             StepId currentAuthoritativeStepId)
{
    self->state = NimbleServerParticipantConnectionStateNormal;
    nimbleServerConnectionQualityReInit(&self->quality);
    nimbleServerConnectionQualityDelayedReset(&self->delayedQuality);
    statsIntInit(&self->incomingStepCountInBufferStats, 60);
    // Expect that the client will add steps for the next authoritative step
    nbsStepsReInit(&self->steps, currentAuthoritativeStepId);
    self->transportConnection = transportConnection;
    self->waitingForReconnectTimer = 0;
    self->warningCount = 0;
    self->warningAboutZeroAddedSteps = 0;
}

/// Facilitates the rejoining process of a participant connection using a new transport connection.
/// @param self Pointer to an instance of NimbleServerParticipantConnection.
/// @param transportConnection The new transport connection through which the participant connection is rejoining.
/// @param currentAuthoritativeStepId The current authoritative step ID that the rejoining process synchronize with.
void nimbleServerParticipantConnectionRejoin(NimbleServerParticipantConnection* self,
                                             NimbleServerTransportConnection* transportConnection,
                                             StepId currentAuthoritativeStepId)
{
    CLOG_C_DEBUG(&self->log, "rejoined from transport connection %hhu at step %08X",
                 transportConnection->transportConnectionId, currentAuthoritativeStepId)
    nimbleServerParticipantConnectionReInit(self, transportConnection, currentAuthoritativeStepId);
}

/// Sets the participant connection in disconnect state.
/// @param self Pointer to an instance of NimbleServerParticipantConnection.
void nimbleServerParticipantConnectionDisconnect(NimbleServerParticipantConnection* self)
{
    CLOG_C_DEBUG(&self->log, "disconnected")
    self->state = NimbleServerParticipantConnectionStateDisconnected;
}

/// Sets the participant connection in a waiting for reconnect state.
/// @param self Pointer to an instance of NimbleServerParticipantConnection.
static void setToWaitingForReconnect(NimbleServerParticipantConnection* self)
{
    CLOG_C_DEBUG(&self->log, "setting state to: waiting for reconnect")
    self->state = NimbleServerParticipantConnectionStateWaitingForReconnect;
    self->waitingForReconnectTimer = 0;
}

/// Monitors the waiting period for a reconnect attempt and determines if it should continue waiting or give up.
/// @param self Pointer to an instance of NimbleServerParticipantConnection
/// @return Returns true if the waiting period has not yet exceeded the maximum allowed time, indicating that it should
/// continue waiting for a possible reconnect. Returns false to suggest that the waiting period has expired,
/// and the connection should be considered for disconnection.
static bool tickWaitingForReconnect(NimbleServerParticipantConnection* self)
{
    self->waitingForReconnectTimer++;
    if (self->waitingForReconnectTimer < self->waitingForReconnectMaxTimer) {
        return true;
    }

    CLOG_C_DEBUG(&self->log, "gave up on reconnect, waited %zu ticks. disconnecting it now",
                 self->waitingForReconnectTimer)

    return false;
}

/// Checks the connection quality of a server participant connection and updates its state accordingly.
/// It decides whether the connection is still viable or if the participant should be moved to a waiting for reconnect
/// state based on the quality assessment.
/// @param self Pointer to an instance of NimbleServerParticipantConnection.
static void tickNormal(NimbleServerParticipantConnection* self)
{
    bool shouldKeep = nimbleServerConnectionQualityDelayedTick(&self->delayedQuality, &self->quality);

    if (!shouldKeep && self->state != NimbleServerParticipantConnectionStateWaitingForReconnect) {
        CLOG_C_DEBUG(&self->log, "connection quality recommended disconnect, so setting it to waiting for reconnect")
        setToWaitingForReconnect(self);
    }
}

/// Updates a participant connection
/// @param self participant connection
/// @return false if connection should be disconnected, true otherwise
bool nimbleServerParticipantConnectionTick(NimbleServerParticipantConnection* self)
{
    switch (self->state) {
        case NimbleServerParticipantConnectionStateNormal:
            tickNormal(self);
            break;
        case NimbleServerParticipantConnectionStateWaitingForReconnect:
            return tickWaitingForReconnect(self);
        case NimbleServerParticipantConnectionStateDisconnected:
            break;
    }

    return true;
}

/// Checks if a participantId is in the participant connection
/// @param self participant connection to check
/// @param participantId participantId to check for
/// @return true if found, false otherwise
bool nimbleServerParticipantConnectionHasParticipantId(const NimbleServerParticipantConnection* self,
                                                       uint8_t participantId)
{
    for (size_t i = 0; i < self->participantReferences.participantReferenceCount; ++i) {
        const NimbleServerParticipant* participant = self->participantReferences.participantReferences[i];
        if (participant->id == participantId) {
            return true;
        }
    }

    return false;
}

int nimbleServerParticipantConnectionDeserializePredictedSteps(NimbleServerParticipantConnection* self,
                                                               FldInStream* inStream)
{
    size_t stepsThatFollow;
    StepId firstStepId;

    int errorCode = nbsStepsInSerializeHeader(inStream, &firstStepId, &stepsThatFollow);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&self->log, "client step: couldn't in-serialize steps")
        return errorCode;
    }
    CLOG_EXECUTE(StepId lastStepId = (StepId) (firstStepId + stepsThatFollow - 1);)

    CLOG_C_VERBOSE(&self->log, "handleIncomingSteps: client %d incoming step range %08X - %08X (count:%zu)", self->id,
                   firstStepId, lastStepId, stepsThatFollow)

    size_t dropped = nbsStepsDropped(&self->steps, firstStepId);
    if (dropped > 0) {
        if (dropped > 60U) {
            CLOG_C_WARN(&self->log, "dropped %zu", dropped)
            return -3;
        }
        CLOG_C_WARN(&self->log, "client step: dropped %zu steps. expected %08X, but got range from %08X to %08X",
                    dropped, self->steps.expectedWriteId, firstStepId, lastStepId)
        nimbleServerInsertForcedSteps(self, dropped);
    }

    int addedStepsCount = nbsStepsInSerialize(inStream, &self->steps, firstStepId, stepsThatFollow);
    if (addedStepsCount < 0) {
        return addedStepsCount;
    }
    if (addedStepsCount == 0) {
        if (self->warningAboutZeroAddedSteps++ % 4 == 0) {
            CLOG_C_DEBUG(&self->log,
                         "Got a packet with old predicted steps. range: %08X - %08X, but is waiting for %08X. (Not a "
                         "problem unless it happens a lot)",
                         firstStepId, lastStepId, self->steps.expectedWriteId)
        }
    }

    if (addedStepsCount > 0) {
        nimbleServerConnectionQualityAddedStepsToBuffer(&self->quality, (size_t) addedStepsCount);
    }

    return 0;
}
