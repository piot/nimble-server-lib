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


    int addedStepsCountOrError = nimbleServerParticipantConnectionDeserializePredictedSteps(foundParticipantConnection, inStream);

    return addedStepsCountOrError;
}
