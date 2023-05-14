/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef UDP_CONNECTIONS_CLIENT_CLIENT_H
#define UDP_CONNECTIONS_CLIENT_CLIENT_H

#include <clog/clog.h>
#include <udp-connections-serialize/serialize.h>
#include <datagram-transport/transport.h>

typedef enum UdpConnectionsClientPhase {
    UdpConnectionsClientPhaseIdle,
    UdpConnectionsClientPhaseChallenge,
    UdpConnectionsClientPhaseConnect,
    UdpConnectionsClientPhaseConnected
} UdpConnectionsClientPhase;

typedef struct UdpConnectionsClient {
    DatagramTransport underlyingTransport;
    DatagramTransport transport;
    UdpConnectionsClientPhase phase;
    UdpConnectionsSerializeClientNonce nonce;
    UdpConnectionsSerializeServerChallenge receivedServerChallenge;
    UdpConnectionsSerializeConnectionId connectionId;
    int waitTime;
    Clog log;
} UdpConnectionsClient;

int udpConnectionsClientInit(UdpConnectionsClient* self, DatagramTransport transport, Clog log);
int udpConnectionsClientUpdate(UdpConnectionsClient* self);

#endif
