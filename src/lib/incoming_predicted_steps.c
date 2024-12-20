/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#include "incoming_predicted_steps.h"
#include <flood/in_stream.h>
#include <inttypes.h>
#include <nimble-server/errors.h>
#include <nimble-server/local_party.h>
#include <nimble-server/transport_connection.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <nimble-steps-serialize/pending_in_serialize.h>

/// Read pending steps from an inStream and move over ready steps to the incoming step buffer
/// @param foundGame game
/// @param inStream stream to read steps from
/// @param transportConnection stream comes from this transport connection
/// @param[out] outClientWaitingForStepId transport connection is waiting for this StepID
/// @return negative on error
int nimbleServerHandleIncomingSteps(NimbleServerGame* foundGame, FldInStream* inStream,
                                    NimbleServerTransportConnection* transportConnection,
                                    StepId* outClientWaitingForStepId)
{
    NimbleServerLocalParty* party = transportConnection->assignedParty;
    if (party == 0) {
        return NimbleServerErrSerialize;
    }
    if (party->state == NimbleServerLocalPartyStateDissolved) {
        party->warningCount++;
        if (party->warningCount % 60 == 0) {
            CLOG_C_NOTICE(&foundGame->log, "ignoring steps from party %u that is dissolved", party->id)
        }
        return NimbleServerErrDatagramFromDisconnectedConnection;
    }

    StepId clientWaitingForStepId;

    (void) foundGame;

    int errorCode = nbsPendingStepsInSerializeHeader(inStream, &clientWaitingForStepId);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&transportConnection->log, "client step: couldn't in-serialize pending steps")
        return errorCode;
    }

    *outClientWaitingForStepId = clientWaitingForStepId;

    CLOG_C_VERBOSE(
        &transportConnection->log,
        "handleIncomingSteps: transport connection %d party: %hhu first predicted StepID %08X",
        transportConnection->transportConnectionId, party->id, clientWaitingForStepId)

    int addedStepsCountOrError = nimbleServerLocalPartyDeserializePredictedSteps(party, inStream);

    return addedStepsCountOrError;
}
