/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>

/// Initializes a participant connection.
/// @param self the participant connection
/// @param transportConnectionIndex the transport connection id
/// @param maxOctets max number of octets to be used for the steps buffer.
/// @param stepId starting StepId for this connection. Game creator will have 0, but later
/// joiners can have another number.
/// @param participants The participants to associate with this connection.
/// @param participantCount number of participants in \p participants.
///
/// Need to create Participants to the game before associating them to the connection.
///
void nbdParticipantConnectionInit(NbdParticipantConnection* self, size_t transportConnectionIndex, ImprintAllocator* connectionAllocator, ImprintAllocatorWithFree* blobAllocator, size_t maxOctets)
{
    nbsStepsInit(&self->steps, connectionAllocator, maxOctets);

    self->participantReferences.participantReferenceCount = 0;
    //self->participants = IMPRINT_ALLOC_TYPE_COUNT(connectionAllocator, NbdParticipant*, 4);

    self->debugCounter = 0;
    self->blobStreamOutAllocator = blobAllocator;
    self->allocatorWithNoFree = connectionAllocator;
    self->nextBlobStreamOutChannel = 127;
    self->transportConnectionId = transportConnectionIndex;
    self->isUsed = false;

    statsIntInit(&self->stepsBehindStats, 60);
    statsIntInit(&self->incomingStepCountInBufferStats, 60);
}

/// Reinitialize the participant connection
/// @param self
/// @param stepId
/// @param participants
/// @param participantCount
void nbdParticipantConnectionReInit(NbdParticipantConnection* self, StepId stepId,
                                    const struct NbdParticipant** participants, size_t participantCount)
{
    self->participantReferences.participantReferenceCount = 0;
    self->nextAuthoritativeStepIdToSend = stepId;
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
