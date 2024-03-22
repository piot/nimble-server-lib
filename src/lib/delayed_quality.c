/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-server/connection_quality.h>
#include <nimble-server/delayed_quality.h>

/// Initialize the connection quality
/// @param self connection quality
/// @param log logging
void nimbleServerConnectionQualityDelayedInit(NimbleServerConnectionQualityDelayed* self, Clog log)
{
    self->log = log;
    self->impedingDisconnectCounter = 0;
}

/// Reset the delayed connection quality
/// @param self delayed connection quality
void nimbleServerConnectionQualityDelayedReset(NimbleServerConnectionQualityDelayed* self)
{
    self->impedingDisconnectCounter = 0;
}

bool nimbleServerConnectionQualityDelayedTick(NimbleServerConnectionQualityDelayed* self,
                                              const NimbleServerConnectionQuality* quality)
{
    bool shouldDisconnect = nimbleServerConnectionQualityCheckIfShouldDisconnect(quality);
    if (shouldDisconnect) {
        if (self->impedingDisconnectCounter == 0) {
#if defined CLOG_LOG_ENABLED
#define BUF_SIZE 128
            char buf[BUF_SIZE];
            CLOG_C_NOTICE(
                &self->log, "quality recommended disconnect for the first time (counter:%zu), description: %s",
                self->impedingDisconnectCounter, nimbleServerConnectionQualityDescribe(quality, buf, BUF_SIZE))

#endif
        }
        self->impedingDisconnectCounter++;
        if (self->impedingDisconnectCounter > 180) {
#if defined CLOG_LOG_ENABLED
            char buf[BUF_SIZE];
            CLOG_C_NOTICE(&self->log, "recommending disconnect (counter:%zu), description: %s",
                          self->impedingDisconnectCounter,
                          nimbleServerConnectionQualityDescribe(quality, buf, BUF_SIZE))

#endif
            return false;
        }

        if (self->impedingDisconnectCounter % 60 == 0) {
#if defined CLOG_LOG_ENABLED
            char buf[BUF_SIZE];
            CLOG_C_NOTICE(&self->log, "bad quality, considering disconnecting (counter:%zu). description: %s",
                          self->impedingDisconnectCounter,
                          nimbleServerConnectionQualityDescribe(quality, buf, BUF_SIZE))
#endif
        }
    } else {
        if (self->impedingDisconnectCounter > 0) {
            self->impedingDisconnectCounter--;
            if (self->impedingDisconnectCounter % 60 == 0) {
                CLOG_C_NOTICE(&self->log, "connection stabilizing (counter:%zu)", self->impedingDisconnectCounter)
            }
            if (self->impedingDisconnectCounter == 0) {
                CLOG_C_NOTICE(&self->log, "connection has stabilized again")
            }
        }
    }

    return true;
}
