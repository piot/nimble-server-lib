/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_CIRCULAR_BUFFER
#define NIMBLE_SERVER_CIRCULAR_BUFFER

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define NIMBLE_SERVER_CIRCULAR_BUFFER_SIZE 64

typedef struct {
    uint8_t data[NIMBLE_SERVER_CIRCULAR_BUFFER_SIZE];
    size_t head;
    size_t tail;
    bool isFull;
} NimbleServerCircularBuffer;

void nimbleServerCircularBufferInit(NimbleServerCircularBuffer* self);
void nimbleServerCircularBufferWrite(NimbleServerCircularBuffer* self, uint8_t data);
uint8_t nimbleServerCircularBufferRead(NimbleServerCircularBuffer* self);
bool nimbleServerCircularBufferIsEmpty(const NimbleServerCircularBuffer* self);
bool nimbleServerCircularBufferIsFull(const NimbleServerCircularBuffer* self);
size_t nimbleServerCircularBufferCount(const NimbleServerCircularBuffer* self);

#endif
