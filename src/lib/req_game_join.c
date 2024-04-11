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
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_join_game.h>
#include <nimble-server/server.h>

static int nimbleServerGameJoinParticipantConnection(NimbleServerParticipantConnections* connections,
                                                     NimbleServerParticipants* gameParticipants,
                                                     NimbleServerTransportConnection* transportConnection,
                                                     const NimbleSerializeJoinGameRequest* joinRequest,
                                                     StepId latestAuthoritativeStepId,
                                                     struct NimbleServerParticipantConnection** outConnection)
{
    NimbleServerParticipantConnection* foundConnection = nimbleServerParticipantConnectionsFindConnectionForTransport(
        connections, transportConnection->transportConnectionId);
    if (foundConnection != 0) {
        CLOG_C_DEBUG(
            &connections->log,
            "found an existing participant connection %u for this transport connection (%hhu). ignoring join request",
            foundConnection->id, transportConnection->transportConnectionId)
        *outConnection = foundConnection;
        return 0;
    }

    switch (joinRequest->joinGameType) {
        case NimbleSerializeJoinGameTypeNoSecret:
            break;
        case NimbleSerializeJoinGameTypeSecret: {
            NimbleServerParticipantConnection*
                foundConnectionFromSecret = nimbleServerParticipantConnectionsFindConnectionFromSecret(
                    connections, joinRequest->connectionSecret);
            if (foundConnectionFromSecret != 0) {
                if (foundConnectionFromSecret->participantReferences.participantReferenceCount !=
                    joinRequest->playerCount) {
                    CLOG_C_NOTICE(&connections->log,
                                  "could not rejoin with secret. wrong number of local participant count")
                } else {
                    CLOG_C_DEBUG(&connections->log, "rejoining, using a secret, to a previous connection %u",
                                 foundConnectionFromSecret->id)
                    nimbleServerParticipantConnectionRejoin(foundConnectionFromSecret, transportConnection,
                                                            latestAuthoritativeStepId);
                    *outConnection = foundConnectionFromSecret;
                    return 0;
                }
                return 0;
            } else {
                CLOG_C_NOTICE(&connections->log,
                              "could not find connection with secret. Probably timed out. trying to add "
                              "participants to a new participant connection")
            }

            break;
        }
        case NimbleSerializeJoinGameTypeHostMigrationParticipantId: {
            NimbleServerParticipantConnection*
                foundConnectionFromParticipantId = nimbleServerParticipantConnectionsFindConnectionFromParticipantId(
                    connections, joinRequest->participantId);
            if (foundConnectionFromParticipantId != 0) {
                if (foundConnectionFromParticipantId->participantReferences.participantReferenceCount !=
                    joinRequest->playerCount) {
                    CLOG_C_NOTICE(&connections->log,
                                  "could not rejoin after host migration participantID. wrong number of local participant count")
                    return -1;
                } else {
                    CLOG_C_DEBUG(&connections->log, "client joining after host migration, requesting participantId %d, to a previous connection %u",
                                 joinRequest->participantId, foundConnectionFromParticipantId->id)
                    nimbleServerParticipantConnectionRejoin(foundConnectionFromParticipantId, transportConnection,
                                                            latestAuthoritativeStepId);
                    *outConnection = foundConnectionFromParticipantId;
                    return 0;
                }
            } else {
                CLOG_C_NOTICE(&connections->log,
                              "could not find connection after host migration with participant id %hhu. Trying fallback and add "
                              "participants to a new participant connection", joinRequest->participantId)
            }
            break;
        }
    }

    int errorCode = nimbleServerParticipantConnectionsCreate(connections, gameParticipants, transportConnection,
                                                             joinRequest->players, latestAuthoritativeStepId,
                                                             joinRequest->playerCount, outConnection);
    if (errorCode < 0) {
        *outConnection = 0;
        return errorCode;
    }

    return 0;
}

static int nimbleServerReadAndJoinParticipants(NimbleServerParticipantConnections* connections,
                                               NimbleServerParticipants* gameParticipants,
                                               NimbleServerTransportConnection* transportConnection,
                                               const NimbleSerializeJoinGameRequest* request,
                                               StepId latestAuthoritativeStepId,
                                               struct NimbleServerParticipantConnection** createdConnection)
{
    int errorCode = nimbleServerGameJoinParticipantConnection(connections, gameParticipants, transportConnection,
                                                              request, latestAuthoritativeStepId,
                                                              createdConnection);
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

    NimbleServerParticipantConnection* createdConnection;
    int errorCode = nimbleServerReadAndJoinParticipants(
        &self->connections, &self->game.participants, transportConnection, &request,
        self->game.authoritativeSteps.expectedWriteId, &createdConnection);
    if (errorCode < 0) {
        CLOG_WARN("couldn't find game session")
        nimbleSerializeServerOutJoinGameOutOfParticipantSlotsResponse(outStream, request.nonce, &self->log);
        return errorCode;
    }
    createdConnection->waitingForReconnectMaxTimer = self->setup.maxWaitingForReconnectTicks;

    transportConnection->assignedParticipantConnection = createdConnection;

    NimbleSerializeJoinGameResponse gameResponse;
    NimbleSerializeJoinGameResponseParticipant* participants = gameResponse.participants;
    for (size_t i = 0; i < createdConnection->participantReferences.participantReferenceCount; ++i) {
        const NimbleServerParticipant* sourceParticipant = createdConnection->participantReferences
                                                               .participantReferences[i];
        participants[i].id = sourceParticipant->id;
        participants[i].localIndex = sourceParticipant->localIndex;
        CLOG_VERBOSE("joined localIndex %zu with ID: %hhu", sourceParticipant->localIndex, sourceParticipant->id)
    }

    gameResponse.participantConnectionIndex = (NimbleSerializeParticipantConnectionIndex) createdConnection->id;
    gameResponse.participantConnectionSecret = createdConnection->secret;
    gameResponse.participantCount = createdConnection->participantReferences.participantReferenceCount;

    CLOG_C_DEBUG(
        &self->log,
        "client joined game with participant connection %u stateID: %04X participant count: %zu secret: %" PRIX64,
        createdConnection->id, self->game.authoritativeSteps.expectedWriteId - 1,
        createdConnection->participantReferences.participantReferenceCount, createdConnection->secret)
    nimbleSerializeServerOutJoinGameResponse(outStream, &gameResponse, &self->log);

    return 0;
}
