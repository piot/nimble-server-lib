/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <nimble-server/default_step.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-steps-serialize/out_serialize.h>

/// Creates a forced step for a participant connection participants.
/// Forced steps are used for a participant connection that is too much behind
/// of other participant connections.
/// @note currently all applications must support a one octet length step with value zero for "empty" forced step.
/// @todo option to copy the last received step and also support application-specific rules for forced steps.
/// @param connection
/// @param forcedStepsBuffer
/// @param maxCount
/// @return
int nbdCreateForcedStep(NbdParticipantConnection* connection, uint8_t* forcedStepsBuffer, size_t maxCount)
{
    static uint8_t buf[1];
    buf[0] = 0;
    NimbleStepsOutSerializeLocalParticipants participants;
    participants.participantCount = connection->participantReferences.participantReferenceCount;
    for (size_t i = 0; i < participants.participantCount; ++i) {
        participants.participants[i].participantId = connection->participantReferences.participantReferences[i]->id;
        participants.participants[i].payload = buf;
        participants.participants[i].payloadCount = 1;
    }
    return nbsStepsOutSerializeStep(&participants, forcedStepsBuffer, maxCount);
}

/// Insert forced steps for a participant connection
///
/// @param foundParticipantConnection
/// @param stepCount number of forced steps to insert
/// @return negative value on error.
int nbdInsertForcedSteps(NbdParticipantConnection* foundParticipantConnection, size_t stepCount)
{
    uint8_t forcedStepsBuffer[64];
    int defaultStepOctetCount = nbdCreateForcedStep(foundParticipantConnection, forcedStepsBuffer, 64);
    StepId stepIdToWrite = foundParticipantConnection->steps.expectedWriteId;
    CLOG_VERBOSE("nbdServer: insert forced steps (0)")
    for (size_t i = 0; i < stepCount; ++i) {
        int errorCode = nbsStepsWrite(&foundParticipantConnection->steps, stepIdToWrite, forcedStepsBuffer,
                                      defaultStepOctetCount);
        if (errorCode < 0) {
            return errorCode;
        }
        stepIdToWrite++;
    }
    return 0;
}
