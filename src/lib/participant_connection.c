/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <inttypes.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/transport_connection.h>
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

    statsIntInit(&self->incomingStepCountInBufferStats, 60);
    // Expect that the client will add steps for the next authoritative step
    nbsStepsReInit(&self->steps, currentAuthoritativeStepId);
    self->transportConnection = transportConnection;
    self->waitingForReconnectTimer = 0;
    self->warningCount = 0;
}

void nimbleServerParticipantConnectionRejoin(NimbleServerParticipantConnection* self,
                                             NimbleServerTransportConnection* transportConnection,
                                             StepId currentAuthoritativeStepId)
{
    CLOG_C_DEBUG(&self->log, "rejoined from transport connection %hhu at step %08X",
                 transportConnection->transportConnectionId, currentAuthoritativeStepId)
    nimbleServerParticipantConnectionReInit(self, transportConnection, currentAuthoritativeStepId);
}

void nimbleServerParticipantConnectionDisconnect(NimbleServerParticipantConnection* self)
{
    CLOG_C_DEBUG(&self->log, "disconnected")
    self->transportConnection->isUsed = false;
    self->state = NimbleServerParticipantConnectionStateDisconnected;
}

static void setToWaitingForReconnect(NimbleServerParticipantConnection* self)
{
    CLOG_C_DEBUG(&self->log, "setting state to: waiting for reconnect")
    self->state = NimbleServerParticipantConnectionStateWaitingForReconnect;
    self->waitingForReconnectTimer = 0;
}

static void updateNormal(NimbleServerParticipantConnection* self)
{
    bool shouldDisconnect = nimbleServerConnectionQualityCheckIfShouldDisconnect(&self->quality);
    if (shouldDisconnect && self->state != NimbleServerParticipantConnectionStateWaitingForReconnect) {
        CLOG_C_DEBUG(&self->log, "connection quality recommended disconnect, so setting it to waiting for reconnect")
        setToWaitingForReconnect(self);
    }
}

static bool updateWaitingForReconnect(NimbleServerParticipantConnection* self)
{
    self->waitingForReconnectTimer++;
    if (self->waitingForReconnectTimer < self->waitingForReconnectMaxTimer) {
        return true;
    }

    CLOG_C_DEBUG(&self->log, "gave up on reconnect, waited %zu ticks. disconnecting it now",
                 self->waitingForReconnectTimer)

    return false;
}

/// Updates a participant connection
/// @param self participant connection
/// @return false if connection should be disconnected, true otherwise
bool nimbleServerParticipantConnectionUpdate(NimbleServerParticipantConnection* self)
{
    switch (self->state) {
        case NimbleServerParticipantConnectionStateNormal:
            updateNormal(self);
            break;
        case NimbleServerParticipantConnectionStateWaitingForReconnect:
            return updateWaitingForReconnect(self);
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
