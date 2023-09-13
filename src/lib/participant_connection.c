/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "nimble-steps-serialize/out_serialize.h"
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/transport_connection.h>

/// Initializes a participant connection.
/// @param self the participant connection
/// @param connectionAllocator allocator for connection collection
/// @param currentAuthoritativeStepId latest authoritative state step ID
/// @param maxParticipantCountForConnection maximum number of participants for a single connection (e.g. for split-screen it is usually 2-4).
/// @param transportConnection the transport connection id
/// @param maxSingleParticipantStepOctetCount max number of octets for one single step.
/// @param log the log to use for logging
/// Need to create Participants to the game before associating them to the connection.
void nimbleServerParticipantConnectionInit(NimbleServerParticipantConnection* self,
                                  NimbleServerTransportConnection* transportConnection,
                                  ImprintAllocator* connectionAllocator, StepId currentAuthoritativeStepId,
                                  size_t maxParticipantCountForConnection, size_t maxSingleParticipantStepOctetCount,
                                  Clog log)
{
    size_t combinedStepOctetSize = nbsStepsOutSerializeCalculateCombinedSize(maxParticipantCountForConnection,
                                                                             maxSingleParticipantStepOctetCount);

    self->log = log;
    nbsStepsInit(&self->steps, connectionAllocator, combinedStepOctetSize, log);
    self->participantReferences.participantReferenceCount = 0;
    self->waitingForReconnectMaxTimer = 62 * 20;
    self->isUsed = false;
    nimbleServerConnectionQualityInit(&self->quality, self->id, log);
    nimbleServerParticipantConnectionReInit(self, transportConnection, currentAuthoritativeStepId);
}

/// Resets the participant connection
/// @param self participant connection
void nimbleServerParticipantConnectionReset(NimbleServerParticipantConnection* self)
{
    self->isUsed = false;
    self->participantReferences.participantReferenceCount = 0;
}



/// Reinitialize the participant connection
/// @param self participant connection
/// @param transportConnection the underlying transport connection
/// @param currentAuthoritativeStepId the authoritative step ID to start from
void nimbleServerParticipantConnectionReInit(NimbleServerParticipantConnection* self, NimbleServerTransportConnection* transportConnection, StepId currentAuthoritativeStepId)
{

    self->state = NimbleServerParticipantConnectionStateNormal;
    nimbleServerConnectionQualityReInit(&self->quality);

    statsIntInit(&self->incomingStepCountInBufferStats, 60);
    // Expect that the client will add steps for the next authoritative step
    nbsStepsReInit(&self->steps, currentAuthoritativeStepId);
    self->transportConnection = transportConnection;
}

/// Checks if a participantId is in the participant connection
/// @param self participant connection to check
/// @param participantId participantId to check for
/// @return true if found, false otherwise
bool nimbleServerParticipantConnectionHasParticipantId(const NimbleServerParticipantConnection* self, uint8_t participantId)
{
    for (size_t i = 0; i < self->participantReferences.participantReferenceCount; ++i) {
        const NimbleServerParticipant* participant = self->participantReferences.participantReferences[i];
        if (participant->id == participantId) {
            return true;
        }
    }

    return false;
}
