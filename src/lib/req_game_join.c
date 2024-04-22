/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <inttypes.h>
#include <nimble-serialize/server_in.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/errors.h>
#include <nimble-server/game.h>
#include <nimble-server/participant.h>
#include <nimble-server/local_party.h>
#include <nimble-server/req_join_game.h>
#include <nimble-server/server.h>

static int joinLocalParty(NimbleServerLocalParties* parties,
                                                     NimbleServerParticipants* gameParticipants,
                                                     NimbleServerTransportConnection* transportConnection,
                                                     const NimbleSerializeJoinGameRequest* joinRequest,
                                                     StepId latestAuthoritativeStepId,
                                                     struct NimbleServerLocalParty** outConnection)
{
    NimbleServerLocalParty* foundConnection = nimbleServerLocalPartiesFindPartyForTransport(
        parties, transportConnection->transportConnectionId);
    if (foundConnection != 0) {
        CLOG_C_DEBUG(
            &parties->log,
            "found an existing party %u for this transport connection (%hhu). ignoring join request",
            foundConnection->id, transportConnection->transportConnectionId)
        *outConnection = foundConnection;
        return 0;
    }

    switch (joinRequest->joinGameType) {
        case NimbleSerializeJoinGameTypeNoSecret:
            break;
        case NimbleSerializeJoinGameTypePartySecret: {
            NimbleServerLocalParty*
                foundPartyFromSecret = nimbleServerLocalPartiesFindParty(
                    parties, joinRequest->partyAndSessionSecret.partyId);
            if (foundPartyFromSecret != 0) {
                if (foundPartyFromSecret->participantReferences.participantReferenceCount !=
                    joinRequest->playerCount) {
                    CLOG_C_NOTICE(&parties->log,
                                  "could not rejoin with secret. wrong number of local participant count")
                } else {
                    CLOG_C_DEBUG(&parties->log, "rejoining, using a secret, to a previous connection %u",
                                 foundPartyFromSecret->id)
                    nimbleServerLocalPartyRejoin(foundPartyFromSecret, transportConnection);
                    *outConnection = foundPartyFromSecret;
                    return 0;
                }
                return 0;
            } else {
                CLOG_C_NOTICE(&parties->log,
                              "could not find connection with secret. Probably timed out. trying to add "
                              "participants to a new party")
            }

            break;
        }
    }

    int errorCode = nimbleServerLocalPartiesCreate(parties, gameParticipants, transportConnection,
                                                             joinRequest->players, latestAuthoritativeStepId,
                                                             joinRequest->playerCount, outConnection);
    if (errorCode < 0) {
        *outConnection = 0;
        return errorCode;
    }

    return 0;
}

static int nimbleServerReadAndJoinParticipants(NimbleServerLocalParties* parties,
                                               NimbleServerParticipants* gameParticipants,
                                               NimbleServerTransportConnection* transportConnection,
                                               const NimbleSerializeJoinGameRequest* request,
                                               StepId latestAuthoritativeStepId,
                                               struct NimbleServerLocalParty** party)
{
    int errorCode = joinLocalParty(parties, gameParticipants, transportConnection,
                                                              request, latestAuthoritativeStepId,
                                                              party);
    if (errorCode < 0) {
        CLOG_WARN("nimbleServerReadAndJoinParticipants: couldn't join game session")
        return errorCode;
    }
    return 0;
}

/// Handles a join request from the client
/// @param self server
/// @param transportConnection transport connection that wants to join
/// @param inStream read join request from this stream
/// @param outStream writes reply to out stream
/// @return negative on error
int nimbleServerReqGameJoin(NimbleServer* self, NimbleServerTransportConnection* transportConnection,
                            FldInStream* inStream, FldOutStream* outStream)
{
    NimbleSerializeJoinGameRequest request;

    int err = nimbleSerializeServerInJoinGameRequest(inStream, &request);
    if (err < 0) {
        return err;
    }

    NimbleServerLocalParty* party;
    int errorCode = nimbleServerReadAndJoinParticipants(
        &self->localParties, &self->game.participants, transportConnection, &request,
        self->game.authoritativeSteps.expectedWriteId, &party);
    if (errorCode < 0) {
        CLOG_WARN("couldn't find game session")
        nimbleSerializeServerOutJoinGameOutOfParticipantSlotsResponse(outStream, request.nonce, &self->log);
        return errorCode;
    }
    party->waitingForReconnectMaxTimer = self->setup.maxWaitingForReconnectTicks;

    transportConnection->assignedParty = party;

    NimbleSerializeJoinGameResponse gameResponse;
    NimbleSerializeJoinGameResponseParticipant* participants = gameResponse.participants;
    for (size_t i = 0; i < party->participantReferences.participantReferenceCount; ++i) {
        const NimbleServerParticipant* sourceParticipant = party->participantReferences
                                                               .participantReferences[i];
        participants[i].participantId = sourceParticipant->id;
        participants[i].localIndex = sourceParticipant->localIndex;
        CLOG_VERBOSE("joined localIndex %zu with ID: %hhu", sourceParticipant->localIndex, sourceParticipant->id)
    }

    gameResponse.partyAndSessionSecret.partyId = party->id;
    gameResponse.partyAndSessionSecret.sessionSecret = self->sessionSecret;
    gameResponse.participantCount = party->participantReferences.participantReferenceCount;

    CLOG_C_DEBUG(
        &self->log,
        "client joined game with party %u stateID: %04X participant count: %zu secret: %" PRIX64,
        party->id, self->game.authoritativeSteps.expectedWriteId - 1,
        party->participantReferences.participantReferenceCount, self->sessionSecret.value)
    nimbleSerializeServerOutJoinGameResponse(outStream, &gameResponse, &self->log);

    return 0;
}
