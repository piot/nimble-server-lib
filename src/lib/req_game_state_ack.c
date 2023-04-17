/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <blob-stream/blob_stream_logic_out.h>
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-serialize/commands.h>
#include <nimble-serialize/serialize.h>
#include <nimble-server/game.h>
#include <nimble-server/req_download_game_state_ack.h>
#include <nimble-server/server.h>
#include <udp-transport/udp_transport.h>
#include <nimble-server/participant_connection.h>

int nbdReqDownloadGameStateAck(NbdParticipantConnection *foundParticipantConnection, NbdTransportConnection* transportConnection, FldInStream* inStream, UdpTransportOut *transportOut) {
    NimbleSerializeBlobStreamChannelId channelId;
    int errorCode = nimbleSerializeInBlobStreamChannelId(inStream, &channelId);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("nbdReqJoinGameStateAck: could not get channelId")
        return errorCode;
    }

    //CLOG_INFO("nbdReqJoinGameStateAck %04X vs %04X", channelId, foundParticipantConnection->blobStreamOutChannel)

    //if (channelId != foundParticipantConnection->blobStreamOutChannel) {
    // CLOG_SOFT_ERROR("we have ack for wrong channel %04X vs %04X", channelId, foundParticipantConnection->blobStreamOutChannel)
    //  return errorCode;
    //}

    int receiveResult = blobStreamLogicOutReceive(&foundParticipantConnection->blobStreamLogicOut, inStream);
    if (receiveResult < 0) {
        CLOG_SOFT_ERROR("nbdReqJoinGameStateAck: could not receive blobStreamLogicOut")
        return receiveResult;
    }

    MonotonicTimeMs now = monotonicTimeMsNow();
    const BlobStreamOutEntry *entries[4];

    int entriesFound = blobStreamLogicOutPrepareSend(&foundParticipantConnection->blobStreamLogicOut, now, entries, 4);
#define UDP_MAX_SIZE (1200)
    static uint8_t buf[UDP_MAX_SIZE];
    FldOutStream stream;

    for (int i = 0; i < entriesFound; ++i) {
        const BlobStreamOutEntry *entry = entries[i];

        // CLOG_DEBUG("sending state %08X (octet count :%zu)", options.stepId, options.gameStateOctetCount);

        fldOutStreamInit(&stream, buf, UDP_MAX_SIZE);
        orderedDatagramOutLogicPrepare(&transportConnection->orderedDatagramOutLogic, &stream);

        nimbleSerializeWriteCommand(&stream, NimbleSerializeCmdGameStatePart, "");
        nimbleSerializeOutBlobStreamChannelId(&stream, channelId);
        blobStreamLogicOutSendEntry(&stream, entry);
        orderedDatagramOutLogicCommit(&transportConnection->orderedDatagramOutLogic);
        transportOut->send(transportOut->self, stream.octets, stream.pos);
    }

    return 0;
}
