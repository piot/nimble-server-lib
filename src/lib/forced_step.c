/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <nimble-server/forced_step.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-steps-serialize/out_serialize.h>

/// Creates a forced step for a participant connection participants.
/// Forced steps are used for a participant connection that is too much behind
/// of other participant connections.
/// @note currently all applications must support a zero octet length step as a zero, "empty" step.
/// @todo option to copy the last received step and also support application-specific rules for forced steps.
/// @param connection
/// @param forcedStepsBuffer
/// @param maxCount
/// @return
int nbdCreateForcedStep(NimbleServerParticipantConnection* connection, uint8_t* forcedStepsBuffer, size_t maxCount)
{
    NimbleStepsOutSerializeLocalParticipants participants;
    participants.participantCount = connection->participantReferences.participantReferenceCount;
    for (size_t i = 0; i < participants.participantCount; ++i) {
        participants.participants[i].participantId = connection->participantReferences.participantReferences[i]->id;
        participants.participants[i].payload = 0;
        participants.participants[i].payloadCount = 0;
    }
    return nbsStepsOutSerializeStep(&participants, forcedStepsBuffer, maxCount);
}

/// Insert forced steps for a participant connection
///
/// @param foundParticipantConnection
/// @param stepCount number of forced steps to insert
/// @return negative value on error.
int nbdInsertForcedSteps(NimbleServerParticipantConnection* foundParticipantConnection, size_t stepCount)
{
    uint8_t forcedStepsBuffer[64];
    StepId stepIdToWrite = foundParticipantConnection->steps.expectedWriteId;
    int forcedStepOctetCount = nbdCreateForcedStep(foundParticipantConnection, forcedStepsBuffer, 64);
    CLOG_VERBOSE("nimbleServer: insert zero input as forced steps (0)")
    for (size_t i = 0; i < stepCount; ++i) {
        int errorCode = nbsStepsWrite(&foundParticipantConnection->steps, stepIdToWrite, forcedStepsBuffer,
                                      forcedStepOctetCount);
        if (errorCode < 0) {
            return errorCode;
        }
        stepIdToWrite++;
    }
    return 0;
}
