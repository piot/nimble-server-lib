/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include "send_authoritative_steps.h"
#include <blob-stream/blob_stream_logic_out.h>
#include <datagram-transport/transport.h>
#include <datagram-transport/types.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-serialize/commands.h>
#include <nimble-serialize/serialize.h>
#include <nimble-server/errors.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_download_game_state_ack.h>
#include <nimble-server/server.h>

/// Handles a download state progress ack from the client
/// @param transportConnection transportConnection
/// @param foundGame the game to send
/// @param inStream stream to read game state ack from
/// @param transportOut the transport to send reply to
/// @return negative on error
int nimbleServerReqDownloadGameStateAck(NimbleServerGame* foundGame,
                                        NimbleServerTransportConnection* transportConnection, FldInStream* inStream,
                                        DatagramTransportOut* transportOut, uint16_t clientTime)
{
    NimbleSerializeBlobStreamChannelId channelId;
    int errorCode = nimbleSerializeInBlobStreamChannelId(inStream, &channelId);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("nimbleServerReqJoinGameStateAck: could not get channelId")
        return errorCode;
    }

    // CLOG_INFO("nimbleServerReqJoinGameStateAck %04X vs %04X", channelId,
    // foundParticipantConnection->blobStreamOutChannel)

    // if (channelId != foundParticipantConnection->blobStreamOutChannel) {
    //  CLOG_SOFT_ERROR("we have ack for wrong channel %04X vs %04X", channelId,
    //  foundParticipantConnection->blobStreamOutChannel)
    //   return errorCode;
    // }

    int receiveResult = blobStreamLogicOutReceive(&transportConnection->blobStreamLogicOut, inStream);
    if (receiveResult < 0) {
        CLOG_SOFT_ERROR("nimbleServerReqJoinGameStateAck: could not receive blobStreamLogicOut")
        return receiveResult;
    }

    MonotonicTimeMs now = monotonicTimeMsNow();
    const BlobStreamOutEntry* entries[4];

    int entriesFound = blobStreamLogicOutPrepareSend(&transportConnection->blobStreamLogicOut, now, entries, 4);
    static uint8_t buf[DATAGRAM_TRANSPORT_MAX_SIZE];
    FldOutStream stream;

    for (int i = 0; i < entriesFound; ++i) {
        const BlobStreamOutEntry* entry = entries[i];

        // CLOG_DEBUG("sending state %08X (octet count :%zu)", options.stepId, options.gameStateOctetCount);

        fldOutStreamInit(&stream, buf, DATAGRAM_TRANSPORT_MAX_SIZE);
        stream.writeDebugInfo = true; // transportConnection->useDebugStreams;

        FldOutStreamStoredPosition finalizePosition;
        transportConnectionPrepareHeader(transportConnection, &stream, clientTime, &finalizePosition);

        nimbleSerializeWriteCommand(&stream, NimbleSerializeCmdGameStatePart, &transportConnection->log);
        nimbleSerializeOutBlobStreamChannelId(&stream, channelId);
        blobStreamLogicOutSendEntry(&stream, entry);

        int finalizeStatus = transportConnectionCommitHeader(transportConnection, &stream, finalizePosition);
        if (finalizeStatus < 0) {
            return finalizeStatus;
        }

        orderedDatagramOutLogicCommit(&transportConnection->orderedDatagramOutLogic);

        if (stream.pos > datagramTransportMaxSize) {
            CLOG_C_SOFT_ERROR(
                &transportConnection->log,
                "trying to send game state part datagram that has too many octets: %zu out of %zu. Discarding it",
                stream.pos, datagramTransportMaxSize)
            return NimbleServerErrSerialize;
        }

        transportOut->send(transportOut->self, stream.octets, stream.pos);
    }

    if (!blobStreamLogicOutIsAllSent(&transportConnection->blobStreamLogicOut)) {
        return 0;
    }

    CLOG_C_DEBUG(&transportConnection->log, "sent all outgoing blob stream")

    // There is a chance that the client will receive the complete blob stream, lets send a packet with
    // authoritative inputs as well

    fldOutStreamInit(&stream, buf, DATAGRAM_TRANSPORT_MAX_SIZE);
    stream.writeDebugInfo = true; // transportConnection->useDebugStreams;
    FldOutStreamStoredPosition secondFinalizePosition;
    transportConnectionPrepareHeader(transportConnection, &stream, clientTime, &secondFinalizePosition);

    CLOG_C_VERBOSE(&transportConnection->log,
                   "download of game state is probably done, send a few authoritative steps as well from %08X",
                   transportConnection->gameState.stepId)

    ssize_t err = nimbleServerSendStepRanges(&stream, transportConnection, foundGame,
                                             transportConnection->gameState.stepId, 0);
    if (err < 0) {
        CLOG_C_SOFT_ERROR(&transportConnection->log, "could not send ranges")
        return (int) err;
    }
    transportConnectionCommitHeader(transportConnection, &stream, secondFinalizePosition);

    if (stream.pos > datagramTransportMaxSize) {
        CLOG_C_SOFT_ERROR(&transportConnection->log,
                          "trying to send auth steps datagram that has too many octets: %zu out of %zu. Discarding it",
                          stream.pos, datagramTransportMaxSize)
        return NimbleServerErrSerialize;
    }

    return transportOut->send(transportOut->self, stream.octets, stream.pos);
}
