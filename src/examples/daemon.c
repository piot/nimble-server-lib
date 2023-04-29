/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-daemon/daemon.h>

int nbdDaemonInit(NbdDaemon* self)
{
    int err = udpServerStartup();
    if (err < 0) {
        return err;
    }

    return udpServerInit(&self->socket, 27000, true);
}
