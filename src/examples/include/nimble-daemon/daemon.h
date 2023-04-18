/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_DAEMON_DAEMON_H
#define NIMBLE_DAEMON_DAEMON_H

#include <nimble-server/server.h>
#include <udp-server/udp_server.h>

typedef struct NbdDaemon {
    NbdServer server;
    UdpServerSocket socket;
} NbdDaemon;

int nbdDaemonInit(NbdDaemon* self);

#endif
