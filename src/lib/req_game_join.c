/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/game.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_join_game.h>
#include <nimble-server/server.h>

static int nbdGameJoinParticipantConnection(NimbleServerParticipantConnections* connections, NimbleServerParticipants* gameParticipants,
                                            size_t transportConnectionId, const NimbleServerParticipantJoinInfo* joinInfo,
                                            StepId latestAuthoritativeStepId, size_t localParticipantCount,
                                            struct NimbleServerParticipantConnection** outConnection)
{
    NimbleServerParticipantConnection* foundConnection = nbdParticipantConnectionsFindConnectionForTransport(
        connections, transportConnectionId);
    if (foundConnection != 0) {
        *outConnection = foundConnection;
        return 0;
    }

    int errorCode = nbdParticipantConnectionsCreate(connections, gameParticipants, transportConnectionId, joinInfo,
                                                    latestAuthoritativeStepId, localParticipantCount, outConnection);
    if (errorCode < 0) {
        *outConnection = 0;
        return errorCode;
    }

    return 0;
}

static int nbdReadAndJoinParticipants(NimbleServerParticipantConnections* connections, NimbleServerParticipants* gameParticipants,
                               size_t transportConnectionId, struct FldInStream* inStream,
                               StepId latestAuthoritativeStepId, struct NimbleServerParticipantConnection** createdConnection)
{
    uint8_t localParticipantCount;
    fldInStreamReadUInt8(inStream, &localParticipantCount);
    CLOG_ASSERT(localParticipantCount > 0, "must have local participants")
    NimbleServerParticipantJoinInfo joinInfos[8];
    for (size_t i = 0; i < localParticipantCount; ++i) {
        fldInStreamReadUInt8(inStream, &joinInfos[i].localIndex);
    }
    int errorCode = nbdGameJoinParticipantConnection(connections, gameParticipants, transportConnectionId, joinInfos,
                                                     latestAuthoritativeStepId, localParticipantCount,
                                                     createdConnection);
    if (errorCode < 0) {
        CLOG_WARN("couldn't join game session")
        return errorCode;
    }
    return 0;
}

/// Handles a join request from the client
/// @param self
/// @param transportConnection
/// @param inStream
/// @param outStream
/// @return
int nbdReqGameJoin(NimbleServer* self, NimbleServerTransportConnection* transportConnection, FldInStream* inStream,
                   FldOutStream* outStream)
{

    NimbleSerializeVersion nimbleProtocolVersion;
    int errorCode = nimbleSerializeInVersion(inStream, &nimbleProtocolVersion);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("could not read nimble serialize version %d", errorCode)
        return errorCode;
    }

    char buf[32];
    CLOG_C_VERBOSE(&transportConnection->log, "request join participants. nimble protocol version %s",
                   nimbleSerializeVersionToString(&nimbleProtocolVersion, buf, 32))

    if (!nimbleSerializeVersionIsEqual(&nimbleProtocolVersion, &g_nimbleProtocolVersion)) {

        CLOG_SOFT_ERROR("wrong version of nimble protocol version. expected %s, but encountered %s",
                        nimbleSerializeVersionToString(&g_nimbleProtocolVersion, buf, 32),
                        nimbleSerializeVersionToString(&nimbleProtocolVersion, buf, 32))
        return -41;
    }

    NimbleSerializeVersion clientApplicationVersion;
    errorCode = nimbleSerializeInVersion(inStream, &clientApplicationVersion);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("wrong version %d", errorCode)
        return errorCode;
    }

    CLOG_C_VERBOSE(&transportConnection->log, "request join participants. application version %s",
                   nimbleSerializeVersionToString(&clientApplicationVersion, buf, 32))

    if (!nimbleSerializeVersionIsEqual(&self->applicationVersion, &clientApplicationVersion)) {
        CLOG_SOFT_ERROR("Wrong application version");
        return -44;
    }

    NimbleServerParticipantConnection* createdConnection;
    errorCode = nbdReadAndJoinParticipants(&self->connections, &self->game.participants,
                                           transportConnection->transportConnectionId, inStream,
                                           self->game.authoritativeSteps.expectedWriteId, &createdConnection);
    if (errorCode < 0) {
        CLOG_WARN("couldn't find game session");
        return errorCode;
    }

    transportConnection->assignedParticipantConnection = createdConnection;

    NimbleSerializeParticipant participants[8];
    for (size_t i = 0; i < createdConnection->participantReferences.participantReferenceCount; ++i) {
        const NimbleServerParticipant* sourceParticipant = createdConnection->participantReferences.participantReferences[i];
        participants[i].id = sourceParticipant->id;
        participants[i].localIndex = sourceParticipant->localIndex;
        CLOG_VERBOSE("joined localIndex %zu with ID: %zu", sourceParticipant->localIndex, sourceParticipant->id)
    }

    CLOG_DEBUG("client joined game with connection %u stateID: %04X participant count: %zu", createdConnection->id,
               self->game.authoritativeSteps.expectedWriteId - 1,
               createdConnection->participantReferences.participantReferenceCount);
    nimbleSerializeServerOutGameJoinResponse(outStream, createdConnection->id, participants,
                                             createdConnection->participantReferences.participantReferenceCount);

    return 0;
}
