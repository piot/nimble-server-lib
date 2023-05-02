/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "nimble-steps-serialize/out_serialize.h"
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>

/// Initializes a participant connection.
/// @param self the participant connection
/// @param transportConnectionIndex the transport connection id
/// @param maxSingleParticipantStepOctetCount max number of octets for one single step.
/// @param stepId starting StepId for this connection. Game creator will have 0, but later
/// joiners can have another number.
/// @param participants The participants to associate with this connection.
/// @param participantCount number of participants in \p participants.
///
/// Need to create Participants to the game before associating them to the connection.
///
void nbdParticipantConnectionInit(NbdParticipantConnection* self, size_t transportConnectionIndex,
                                  ImprintAllocator* connectionAllocator, StepId currentAuthoritativeStepId,
                                  size_t maxParticipantCountForConnection, size_t maxSingleParticipantStepOctetCount,
                                  Clog log)
{
    size_t combinedStepOctetSize = nbsStepsOutSerializeCalculateCombinedSize(maxParticipantCountForConnection,
                                                                             maxSingleParticipantStepOctetCount);

    self->log = log;
    nbsStepsInit(&self->steps, connectionAllocator, combinedStepOctetSize, log);
    // Expect that the client will add steps for the next authoritative step
    nbsStepsReInit(&self->steps, currentAuthoritativeStepId);
    self->participantReferences.participantReferenceCount = 0;

    self->allocatorWithNoFree = connectionAllocator;
    self->transportConnectionId = transportConnectionIndex;
    self->isUsed = false;

    statsIntInit(&self->incomingStepCountInBufferStats, 60);
}

/// Reinitialize the participant connection
/// @param self
/// @param stepId
/// @param participants
/// @param participantCount
void nbdParticipantConnectionReInit(NbdParticipantConnection* self, const struct NbdParticipant** participants,
                                    size_t participantCount)
{
    self->participantReferences.participantReferenceCount = 0;
}

void nbdParticipantConnectionReset(NbdParticipantConnection* self)
{
    self->isUsed = false;
    self->participantReferences.participantReferenceCount = 0;
}

int nbdParticipantConnectionHasParticipantId(const NbdParticipantConnection* self, uint8_t participantId)
{
    for (size_t i = 0; i < self->participantReferences.participantReferenceCount; ++i) {
        const NbdParticipant* participant = self->participantReferences.participantReferences[i];
        if (participant->id == participantId) {
            return 1;
        }
    }

    return 0;
}