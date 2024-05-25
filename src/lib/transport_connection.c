/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <nimble-server/transport_connection.h>

/// Initializes a transport connection
/// Holds information for a specified connection in the transport
/// @param self transport connection
/// @param blobStreamAllocator allocator for the blob stream
/// @param log target logging
void transportConnectionInit(NimbleServerTransportConnection* self, ImprintAllocatorWithFree* blobStreamAllocator,
                             size_t maxGameStateOctetSize, Clog log)
{
    self->log = log;

    orderedDatagramOutLogicInit(&self->orderedDatagramOutLogic);
    orderedDatagramInLogicInit(&self->orderedDatagramInLogic);
    CLOG_C_DEBUG(&self->log, "transport connection allocating for maxGameState: %zu", maxGameStateOctetSize)
    nimbleServerGameStateInit(&self->gameState, &blobStreamAllocator->allocator, maxGameStateOctetSize);

    self->nextBlobStreamOutChannel = 127;
    self->blobStreamOutAllocator = blobStreamAllocator;
    self->debugCounter = 0;
    self->isUsed = true;
    self->noRangesToSendCounter = 0;
    self->phase = NbTransportConnectionPhaseIdle;
    self->blobStreamOutClientRequestId = 0;
    self->useDebugStreams = true;

    statsIntInit(&self->stepsBehindStats, 60);
}

void transportConnectionDisconnect(NimbleServerTransportConnection* self)
{
    CLOG_C_DEBUG(&self->log, "disconnecting transport connection %hhu", self->id)
    self->isUsed = false;
    self->phase = NbTransportConnectionPhaseDisconnected;
}
/// sets the latest authoritative state tick id
/// @param self transport connection
void transportConnectionSetGameStateTickId(NimbleServerTransportConnection* self)
{
    self->phase = NbTransportConnectionPhaseInitialStateDetermined;
}



int transportConnectionPrepareHeader(NimbleServerTransportConnection* self, FldOutStream* outStream,
                                     uint16_t clientTime, FldOutStreamStoredPosition* seekPosition)
{
    *seekPosition = fldOutStreamTell(outStream);
    connectionLayerOutgoingWrite(&self->outgoingConnection, outStream, 0, 0);

    fldOutStreamWriteUInt8(outStream, self->id);
    orderedDatagramOutLogicPrepare(&self->orderedDatagramOutLogic, outStream);
    return fldOutStreamWriteUInt16(outStream, clientTime);
}

int transportConnectionCommitHeader(NimbleServerTransportConnection* self, FldOutStream* outStream,
                                    FldOutStreamStoredPosition seekPosition)
{
    FldOutStreamStoredPosition restoreEndPosition = fldOutStreamTell(outStream);
    size_t octetCount = outStream->pos;
    fldOutStreamSeek(outStream, seekPosition);
const int connectionLayerOctetCount = 1+1+4;
    int writeStatus = connectionLayerOutgoingWrite(&self->outgoingConnection, outStream, outStream->p + connectionLayerOctetCount,
                                                   octetCount - outStream->pos - connectionLayerOctetCount);
    if (writeStatus < 0) {
        return writeStatus;
    }

    fldOutStreamSeek(outStream, restoreEndPosition);

    return 0;
}
