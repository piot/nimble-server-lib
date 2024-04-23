//
// Created by Peter Björklund on 2024-04-22.
//
#include <nimble-server/local_party.h>
#include <nimble-server/participant.h>
#include <nimble-steps-serialize/in_serialize.h>

void nimbleServerParticipantInit(NimbleServerParticipant* self, NimbleServerParticipantSetup setup)
{
    self->log = setup.log;
    self->id = setup.id;
    nbsStepsInit(&self->steps, setup.connectionAllocator, setup.maxStepOctetSizeForOneParticipant, setup.log);
}

void nimbleServerParticipantReInit(NimbleServerParticipant* self, NimbleServerLocalParty* party,
                                   StepId currentAuthoritativeStepId)
{
    nbsStepsReInit(&self->steps, currentAuthoritativeStepId);
    self->inParty = party;
}

int nimbleServerParticipantDeserializeSingleStep(NimbleServerParticipant* self, StepId stepId,
                                                 struct FldInStream* inStream)
{
    return nbsStepsInSerializeSinglePredictedStep(inStream, stepId, &self->steps);
}
