/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <nimble-server/circular_buffer.h>

void nimbleServerCircularBufferInit(NimbleServerCircularBuffer* self)
{
    self->head = 0;
    self->tail = 0;
    self->isFull = false;
}

void nimbleServerCircularBufferWrite(NimbleServerCircularBuffer* self, uint8_t data)
{
    if (self->isFull) {
        CLOG_ERROR("overwriting circular buffer")
        // return;
    }

    self->data[self->head] = data;
    self->head = (self->head + 1) % NIMBLE_SERVER_CIRCULAR_BUFFER_SIZE;

    self->isFull = (self->head == self->tail);
}

uint8_t nimbleServerCircularBufferRead(NimbleServerCircularBuffer* self)
{
    if (self->head == self->tail && !self->isFull) {
        CLOG_ERROR("buffer was empty")
        // return;
    }

    uint8_t data = self->data[self->tail];
    self->tail = (self->tail + 1) % NIMBLE_SERVER_CIRCULAR_BUFFER_SIZE;
    self->isFull = false;

    return data;
}

bool nimbleServerCircularBufferIsEmpty(const NimbleServerCircularBuffer* self)
{
    return (self->head == self->tail) && !self->isFull;
}

bool nimbleServerCircularBufferIsFull(const NimbleServerCircularBuffer* self)
{
    return self->isFull;
}

size_t nimbleServerCircularBufferCount(const NimbleServerCircularBuffer* self)
{
    if (self->isFull) {
        return NIMBLE_SERVER_CIRCULAR_BUFFER_SIZE;
    }

    if (self->head >= self->tail) {
        return self->head - self->tail;
    } else {
        return NIMBLE_SERVER_CIRCULAR_BUFFER_SIZE + self->head - self->tail;
    }
}
