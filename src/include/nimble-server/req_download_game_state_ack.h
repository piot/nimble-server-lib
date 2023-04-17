/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_ACK_H
#define NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_ACK_H

#include <stddef.h>
#include <stdint.h>

struct NbdParticipantConnection;
struct NbdTransportConnection;
struct UdpTransportOut;
struct FldInStream;

int nbdReqDownloadGameStateAck(struct NbdParticipantConnection* self, struct NbdTransportConnection* transportConnection, struct FldInStream* inStream,
                    struct UdpTransportOut *transportOut);

#endif
