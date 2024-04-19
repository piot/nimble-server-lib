/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <nimble-server/forced_step.h>
#include <nimble-server/participant.h>
#include <nimble-server/local_party.h>
#include <nimble-steps-serialize/out_serialize.h>

static NimbleSerializeStepType forcedStateFromConnection(NimbleServerLocalParty* connection)
{
    NimbleSerializeStepType state = NimbleSerializeStepTypeStepNotProvidedInTime;
    if (connection->state == NimbleServerLocalPartyStateWaitingForReJoin) {
        state = NimbleSerializeStepTypeWaitingForReJoin;
    }

    return state;
}

/// Creates a forced step for a party participants.
/// Forced steps are used for a party that is too much behind
/// of other partys.
/// @note currently all applications must support a zero octet length step as a zero, "empty" step.
/// @todo option to copy the last received step and also support application-specific rules for forced steps.
/// @param party party
/// @param forcedStepsBuffer buffer to write forced step to
/// @param maxCount maximum count of forced step buffer
/// @return negative on error
ssize_t nimbleServerCreateForcedStep(NimbleServerLocalParty* party, uint8_t* forcedStepsBuffer,
                                     size_t maxCount)
{
    NimbleStepsOutSerializeLocalParticipants participants;

    NimbleSerializeStepType state = forcedStateFromConnection(party);
    participants.participantCount = party->participantReferences.participantReferenceCount;
    for (size_t i = 0; i < participants.participantCount; ++i) {
        participants.participants[i]
            .participantId =  party->participantReferences.participantReferences[i]->id;
        participants.participants[i].connectState = state;
        participants.participants[i].payload = 0;
        participants.participants[i].payloadCount = 0;
    }
    return nbsStepsOutSerializeStep(&participants, forcedStepsBuffer, maxCount);
}

/// Insert forced steps for a party
/// @param party party to insert forced step to
/// @param stepCount number of forced steps to insert
/// @return negative value on error.
int nimbleServerInsertForcedSteps(NimbleServerLocalParty* party, size_t stepCount)
{
    uint8_t forcedStepsBuffer[64];
    StepId stepIdToWrite = party->steps.expectedWriteId;
    ssize_t forcedStepOctetCount = nimbleServerCreateForcedStep(party, forcedStepsBuffer, 64);
    if (forcedStepOctetCount < 0) {
        return (int) forcedStepOctetCount;
    }
    CLOG_VERBOSE("nimbleServer: insert zero input as forced steps (0)")
    for (size_t i = 0; i < stepCount; ++i) {
        int errorCode = nbsStepsWrite(&party->steps, stepIdToWrite, forcedStepsBuffer,
                                      (size_t) forcedStepOctetCount);
        if (errorCode < 0) {
            return errorCode;
        }
        stepIdToWrite++;
    }
    return 0;
}
