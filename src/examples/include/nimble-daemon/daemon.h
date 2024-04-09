/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_DAEMON_DAEMON_H
#define NIMBLE_DAEMON_DAEMON_H

#include <nimble-server/server.h>
#include <udp-server/udp_server.h>

typedef struct NimbleServerDaemon {
    NimbleServer server;
    UdpServerSocket socket;
} NimbleServerDaemon;

int nimbleServerDaemonInit(NimbleServerDaemon* self);

#endif
