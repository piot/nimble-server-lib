/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <nimble-server/local_party.h>
#include <nimble-server/participant.h>
#include <nimble-steps-serialize/in_serialize.h>

/// Prepares, initializes and allocates memory for a participant
/// @param self the participant to initialize
/// @param setup the parameter for the participant (allocator and max step octet size).
void nimbleServerParticipantInit(NimbleServerParticipant* self, NimbleServerParticipantSetup setup)
{
    self->log = setup.log;
    self->id = setup.id;
    self->isUsed = false;
    self->state = NimbleServerParticipantStateDestroyed;
    nbsStepsInit(&self->steps, setup.connectionAllocator, setup.maxStepOctetSizeForOneParticipant, setup.log);
}

/// ReInitializes the same allocated memory
/// @param self the participant to reinitialize
/// @param party the party that the participant is assigned to. can not be NULL.
/// @param currentAuthoritativeStepId the last composed authoritative step
void nimbleServerParticipantReInit(NimbleServerParticipant* self, NimbleServerLocalParty* party,
                                   StepId currentAuthoritativeStepId)
{
    CLOG_ASSERT(party != 0, "party must be valid")
    nbsStepsReInit(&self->steps, currentAuthoritativeStepId);
    self->inParty = party;
    self->isUsed = true;
    self->state = NimbleServerParticipantStateJustJoined;
}

/// Destroys the participant (marks the memory as not used)
/// @param self participant to mark as not used.
void nimbleServerParticipantDestroy(NimbleServerParticipant* self)
{
    self->isUsed = false;
    self->inParty = 0;
    self->localIndex = 0;
    self->state = NimbleServerParticipantStateDestroyed;
}

/// Marking the participant as leaving
/// @param self participant
void nimbleServerParticipantMarkAsLeaving(NimbleServerParticipant* self)
{
    self->state = NimbleServerParticipantStateLeaving;
}

int nimbleServerParticipantDeserializeSingleStep(NimbleServerParticipant* self, StepId stepId,
                                                 struct FldInStream* inStream)
{
    return nbsStepsInSerializeSinglePredictedStep(inStream, stepId, &self->steps);
}
