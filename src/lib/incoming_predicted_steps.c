/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "incoming_predicted_steps.h"
#include <flood/in_stream.h>
#include <inttypes.h>
#include <nimble-server/errors.h>
#include <nimble-server/forced_step.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/transport_connection.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <nimble-steps-serialize/pending_in_serialize.h>

/// Read pending steps from an inStream and move over ready steps to the incoming step buffer
/// @param foundGame game
/// @param inStream stream to read steps from
/// @param transportConnection stream comes from this transport connection
/// @param[out] outClientWaitingForStepId transport connection is waiting for this StepID
/// @param[out] outReceiveMask transport connection reported this receive mask
/// @return negative on error
int nimbleServerHandleIncomingSteps(NimbleServerGame* foundGame, FldInStream* inStream,
                                    NimbleServerTransportConnection* transportConnection,
                                    StepId* outClientWaitingForStepId, uint64_t* outReceiveMask)
{
    NimbleServerParticipantConnection* foundParticipantConnection = transportConnection->assignedParticipantConnection;
    if (foundParticipantConnection == 0) {
        return NimbleServerErrSerialize;
    }
    if (foundParticipantConnection->state == NimbleServerParticipantConnectionStateDisconnected) {
        foundParticipantConnection->warningCount++;
        if (foundParticipantConnection->warningCount % 60 == 0) {
            CLOG_C_NOTICE(&foundGame->log, "ignoring steps from connection %u that is disconnected",
                          foundParticipantConnection->id)
        }
        return NimbleServerErrDatagramFromDisconnectedConnection;
    }

    StepId clientWaitingForStepId;
    uint64_t receiveMask;

    (void) foundGame;

    int errorCode = nbsPendingStepsInSerializeHeader(inStream, &clientWaitingForStepId, &receiveMask);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&transportConnection->log, "client step: couldn't in-serialize pending steps")
        return errorCode;
    }

    *outClientWaitingForStepId = clientWaitingForStepId;
    *outReceiveMask = receiveMask;

    CLOG_C_VERBOSE(&transportConnection->log,
                   "handleIncomingSteps: client %d is awaiting step %08X receiveMask: %" PRIX64,
                   transportConnection->transportConnectionId, clientWaitingForStepId, receiveMask)

    size_t stepsThatFollow;
    StepId firstStepId;
    errorCode = nbsStepsInSerializeHeader(inStream, &firstStepId, &stepsThatFollow);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&transportConnection->log, "client step: couldn't in-serialize steps")
        return errorCode;
    }

    CLOG_C_VERBOSE(&foundParticipantConnection->log,
                   "handleIncomingSteps: client %d incoming step %08X, %zu steps follow",
                   foundParticipantConnection->id, firstStepId, stepsThatFollow)

    size_t dropped = nbsStepsDropped(&foundParticipantConnection->steps, firstStepId);
    if (dropped > 0) {
        if (dropped > 60U) {
            CLOG_C_WARN(&foundParticipantConnection->log, "dropped %zu", dropped)
            return -3;
        }
        CLOG_C_WARN(&foundParticipantConnection->log,
                    "client step: dropped %zu steps. expected %08X, but got from %u to %zu", dropped,
                    foundParticipantConnection->steps.expectedWriteId, firstStepId, firstStepId + stepsThatFollow - 1)
        nimbleServerInsertForcedSteps(foundParticipantConnection, dropped);
    }

    int addedStepsCount = nbsStepsInSerialize(inStream, &foundParticipantConnection->steps, firstStepId,
                                              stepsThatFollow);
    if (addedStepsCount < 0) {
        return addedStepsCount;
    }

    if (addedStepsCount > 0) {
        nimbleServerConnectionQualityAddedStepsToBuffer(&foundParticipantConnection->quality, (size_t) addedStepsCount);
    }

    return addedStepsCount;
}
