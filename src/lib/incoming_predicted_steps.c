/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "incoming_predicted_steps.h"
#include <flood/in_stream.h>
#include <nimble-server/forced_step.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/transport_connection.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <nimble-steps-serialize/pending_in_serialize.h>

int nimbleServerHandleIncomingSteps(NimbleServerGame* foundGame, FldInStream* inStream,
                                    NimbleServerTransportConnection* transportConnection,
                                    StepId* outClientWaitingForStepId, uint64_t* outReceiveMask,
                                    uint16_t* receivedTimeFromClient)
{

    StepId clientWaitingForStepId;
    uint64_t receiveMask;

    int errorCode = nbsPendingStepsInSerializeHeader(inStream, &clientWaitingForStepId, &receiveMask,
                                                     receivedTimeFromClient);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&transportConnection->log, "client step: couldn't in-serialize pending steps")
        return errorCode;
    }

    *outClientWaitingForStepId = clientWaitingForStepId;
    *outReceiveMask = receiveMask;

    CLOG_C_VERBOSE(&transportConnection->log, "handleIncomingSteps: client %d is awaiting step %08X receiveMask: %08X",
                   transportConnection->transportConnectionId, clientWaitingForStepId, receiveMask)

    size_t stepsThatFollow;
    StepId firstStepId;
    errorCode = nbsStepsInSerializeHeader(inStream, &firstStepId, &stepsThatFollow);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&transportConnection->log, "client step: couldn't in-serialize steps")
        return errorCode;
    }

    NimbleServerParticipantConnection* foundParticipantConnection = transportConnection->assignedParticipantConnection;
    if (foundParticipantConnection == 0) {
        return 0;
    }
    if (foundParticipantConnection->state != NimbleServerParticipantConnectionStateNormal) {
        CLOG_C_NOTICE(&foundGame->log, "ignoring steps from connection %zu that is disconnected",
                      foundParticipantConnection->id)
        return -2;
    }

    CLOG_C_VERBOSE(&foundParticipantConnection->log,
                   "handleIncomingSteps: client %d incoming step %08X, %zu steps follow",
                   foundParticipantConnection->id, firstStepId, stepsThatFollow)

    size_t dropped = nbsStepsDropped(&foundParticipantConnection->steps, firstStepId);
    if (dropped > 0) {
        if (dropped > 60U) {
            CLOG_C_WARN(&foundParticipantConnection->log, "dropped %d", dropped)
            return -3;
        }
        CLOG_C_WARN(&foundParticipantConnection->log,
                    "client step: dropped %zu steps. expected %08X, but got from %08X to %08X", dropped,
                    foundParticipantConnection->steps.expectedWriteId, firstStepId, firstStepId + stepsThatFollow - 1)
        nbdInsertForcedSteps(foundParticipantConnection, dropped);
    }

    int addedStepsCount = nbsStepsInSerialize(inStream, &foundParticipantConnection->steps, firstStepId,
                                              stepsThatFollow);
    if (addedStepsCount < 0) {
        return addedStepsCount;
    }

    return addedStepsCount;
}