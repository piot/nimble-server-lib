/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-server/default_step.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-steps-serialize/out_serialize.h>
#include <clog/clog.h>

int nbdCreateDefaultStep(NbdParticipantConnection* connection, uint8_t* defaultStepBuffer, size_t maxCount)
{
    static uint8_t buf[1];
    buf[0] = 0;
    NimbleStepsOutSerializeLocalParticipants participants;
    participants.participantCount = connection->participantReferences.participantReferenceCount;
    for (size_t i = 0; i < participants.participantCount; ++i) {
        participants.participants[i].participantIndex = connection->participantReferences.participantReferences[i]->id;
        participants.participants[i].payload = buf;
        participants.participants[i].payloadCount = 1;
    }
    return nbsStepsOutSerializeStep(&participants, defaultStepBuffer, maxCount);
}

int nbdInsertDefaultSteps(NbdParticipantConnection* foundParticipantConnection, size_t dropped)
{
    uint8_t defaultStepBuffer[64];
    int defaultStepOctetCount = nbdCreateDefaultStep(foundParticipantConnection, defaultStepBuffer, 64);
    StepId stepIdToWrite = foundParticipantConnection->steps.expectedWriteId;
    CLOG_VERBOSE("nbdServer: insert default steps (0)")
    for (size_t i = 0; i < dropped; ++i) {
        int errorCode = nbsStepsWrite(&foundParticipantConnection->steps, stepIdToWrite, defaultStepBuffer,
                                      defaultStepOctetCount);
        if (errorCode < 0) {
            return errorCode;
        }
        stepIdToWrite++;
    }
    return 0;
}
