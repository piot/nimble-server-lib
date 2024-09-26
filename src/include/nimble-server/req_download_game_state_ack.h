/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_ACK_H
#define NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_ACK_H

#include <stddef.h>
#include <stdint.h>

struct NimbleServerLocalParty;
struct NimbleServerTransportConnection;
struct DatagramTransportOut;
struct FldInStream;

int nimbleServerReqBlobStream(struct NimbleServerGame* game,
                                        struct NimbleServerTransportConnection* transportConnection,
                                        struct FldInStream* inStream, struct DatagramTransportOut* transportOut,
                                        uint16_t clientTime);

int nimbleServerSendBlobStream(struct NimbleServerTransportConnection* transportConnection,
                               struct DatagramTransportOut* transportOut, uint16_t clientTime);

#endif
