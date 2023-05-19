# Nimble Server lib

Server implementing the [Nimble Protocol](https://github.com/piot/nimble-serialize-c/blob/main/docs/index.adoc).

Nimble Server [weaves together](https://github.com/piot/nimble-server-lib/blob/main/docs/index.adoc) authoritative steps from the predicted steps sent by the clients, as well as provide a recently stored game state to joining clients. The game state is provided by one of the clients, since the Nimble Server does not do any simulation.

## Usage

### Initialize

```c
typedef struct NimbleServerSetup {
    NimbleSerializeVersion applicationVersion;
    struct ImprintAllocator* memory;
    size_t maxParticipantConnectionCount;
    size_t maxParticipantCount;
    size_t maxSingleParticipantStepOctetCount;
    size_t maxParticipantCountForEachConnection;
    size_t maxGameStateOctetCount;
    DatagramTransportMulti multiTransport;
    MonotonicTimeMs now;
    Clog log;
} NimbleServerSetup;

int nimbleServerInit(NimbleServer* self, NimbleServerSetup setup);
```


### Update

```c
int nimbleServerUpdate(NimbleServer* self, MonotonicTimeMs now);
```

### Local Usage

if Server Library is used embedded in a client, call `nimbleServerMustProvideGameState` every tick:

```c
bool nimbleServerMustProvideGameState(const NimbleServer* self);
```

and if it returns true, set the latest authoritative state:

```c
void nimbleServerSetGameState(NimbleServer* self, const uint8_t* gameState, size_t gameStateOctetCount, StepId stepId);
```
