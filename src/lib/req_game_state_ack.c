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
#include <nimble-server/local_party.h>
#include <nimble-server/req_download_game_state_ack.h>
#include <nimble-server/server.h>

/// Handles a download state progress ack from the client
/// @param transportConnection transportConnection
/// @param foundGame the game to send
/// @param inStream stream to read game state ack from
/// @param transportOut the transport to send reply to
/// @return negative on error
int nimbleServerReqBlobStream(NimbleServerGame* foundGame,
                                        NimbleServerTransportConnection* transportConnection, FldInStream* inStream,
                                        DatagramTransportOut* transportOut)
{
    (void) foundGame;

    // CLOG_INFO("nimbleServerReqJoinGameStateAck %04X vs %04X", channelId,
    // party->blobStreamOutChannel)

    // if (channelId != party->blobStreamOutChannel) {
    //  CLOG_SOFT_ERROR("we have ack for wrong channel %04X vs %04X", channelId,
    //  party->blobStreamOutChannel)
    //   return errorCode;
    // }

    int receiveResult = blobStreamLogicOutReceive(&transportConnection->blobStreamLogicOut, inStream);
    if (receiveResult < 0) {
        CLOG_SOFT_ERROR("nimbleServerReqJoinGameStateAck: could not receive blobStreamLogicOut")
        return receiveResult;
    }

    return nimbleServerSendBlobStream(transportConnection, transportOut);
}

/*
int sendAuthoritativeSteps() {
    // There is a chance that the client will receive the complete blob stream, lets send a packet with
    // authoritative inputs as well

    fldOutStreamInit(&stream, buf, DATAGRAM_TRANSPORT_MAX_SIZE);
    stream.writeDebugInfo = true; // transportConnection->useDebugStreams;
    int error = transportConnectionWriteHeader(transportConnection, &stream, clientTime);
    if (error < 0) {
        return error;
    }

    CLOG_C_VERBOSE(&transportConnection->log,
                   "download of game state is probably done, send a few authoritative steps as well from %08X",
                   transportConnection->gameState.stepId)

    ssize_t err = nimbleServerSendStepRanges(&stream, transportConnection, foundGame,
                                             transportConnection->gameState.stepId, 0);
    if (err < 0) {
        CLOG_C_SOFT_ERROR(&transportConnection->log, "could not send ranges")
        return (int) err;
    }

    if (stream.pos > datagramTransportMaxSize) {
        CLOG_C_SOFT_ERROR(&transportConnection->log,
                          "trying to send auth steps datagram that has too many octets: %zu out of %zu. Discarding it",
                          stream.pos, datagramTransportMaxSize)
        return NimbleServerErrSerialize;
    }

    return transportOut->send(transportOut->self, stream.octets, stream.pos);
}
*/

int nimbleServerSendBlobStream(NimbleServerTransportConnection* transportConnection, DatagramTransportOut* transportOut)
{
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

        transportConnectionWriteHeader(transportConnection, &stream);

        // Signals that it is a blob stream command that follows
        nimbleSerializeWriteCommand(&stream, NimbleSerializeCmdServerOutBlobStream, &transportConnection->log);

        blobStreamLogicOutSendEntry(&stream, entry, transportConnection->blobStreamLogicOut.transferId);

        transportConnectionCommitHeader(transportConnection);

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
    return 0;
}
