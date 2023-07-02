/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <inttypes.h>
#include <secure-random/secure_random.h>
#include <udp-connections-client/client.h>
#include <udp-connections-serialize/client_in.h>
#include <udp-connections-serialize/client_out.h>

/// Send packet to underlying transport with the out packet header.
/// @param _self udp connections client
/// @param data datagram payload to send
/// @param octetCount number of octets in data
/// @return negative on error
static int transportSend(void* _self, const uint8_t* data, size_t octetCount)
{
    UdpConnectionsClient* self = (UdpConnectionsClient*) _self;
    if (self->phase != UdpConnectionsClientPhaseConnected) {
        CLOG_C_SOFT_ERROR(&self->log, "can not send, we are not connected yet")
        return -55;
    }
    uint8_t buf[1210];
    FldOutStream outStream;

    fldOutStreamInit(&outStream, buf, 1210);

    udpConnectionsSerializeClientOutPacketHeader(&outStream, self->connectionId, (uint16_t) octetCount);

    fldOutStreamWriteOctets(&outStream, data, octetCount);

    return datagramTransportSend(&self->underlyingTransport, buf, outStream.pos);
}

/// Handle incoming Challenge Response from server
/// @param self udp connections client
/// @param inStream stream to read challenge from
/// @return negative on error
static int inChallenge(UdpConnectionsClient* self, FldInStream* inStream)
{
    UdpConnectionsSerializeServerChallenge challenge;
    UdpConnectionsSerializeClientNonce nonce;

    int err = udpConnectionsSerializeClientInChallengeResponse(inStream, &nonce, &challenge);
    if (err < 0) {
        return err;
    }

    if (nonce != self->nonce) {
        CLOG_C_NOTICE(&self->log, "wrong nonce, it wasn't for me. received %" PRIX64 " vs %" PRIX64 ".", nonce,
                      self->nonce)
        return -2;
    }

    if (self->phase != UdpConnectionsClientPhaseChallenge) {
        CLOG_C_NOTICE(&self->log, "ignoring challenge response, we are not in that phase")
        return 0;
    }

    self->receivedServerChallenge = challenge;
    self->phase = UdpConnectionsClientPhaseConnect;

    CLOG_C_DEBUG(&self->log, "received for nonce %" PRIX64 " challenge secret %" PRIX64 " from server", self->nonce,
                 self->receivedServerChallenge)

    return 0;
}

/// Handle incoming connect response from server
/// @param self udp connections client
/// @param inStream connect info to read from
/// @return negative on error
static int inConnect(UdpConnectionsClient* self, FldInStream* inStream)
{
    UdpConnectionsSerializeConnectionId connectionId;
    UdpConnectionsSerializeClientNonce nonce;

    int err = udpConnectionsSerializeClientInConnectResponse(inStream, &nonce, &connectionId);
    if (err < 0) {
        return err;
    }

    if (nonce != self->nonce) {
        CLOG_C_NOTICE(&self->log, "wrong nonce received %" PRIX64 " vs %" PRIX64 ".", nonce, self->nonce)
        return -2;
    }

    if (self->phase != UdpConnectionsClientPhaseConnect) {
        CLOG_C_NOTICE(&self->log, "ignoring connect response, we are not in that phase")
        return 0;
    }

    self->connectionId = connectionId;
    self->phase = UdpConnectionsClientPhaseConnected;

    CLOG_C_DEBUG(&self->log, "connection established from server for client nonce: %" PRIX64 " connection %" PRIX64,
                 self->nonce, self->connectionId)

    return 0;
}

/// Handle challenge or connection response from server.
/// @param self udp connections client
/// @param inStream stream to read incoming datagrams from
/// @param cmd UDP Connections Command
/// @return negative on error
static int handleInStream(UdpConnectionsClient* self, FldInStream* inStream, uint8_t cmd)
{
    switch (cmd) {
        case UdpConnectionsSerializeCmdChallengeResponse:
            return inChallenge(self, inStream);
        case UdpConnectionsSerializeCmdConnectResponse:
            return inConnect(self, inStream);
        default:
            CLOG_C_SOFT_ERROR(&self->log, "illegal cmd: %02X", cmd)
            return -99;
    }
}

/// Handle incoming packet from server to be read by the application
/// @param self udp connections
/// @param inStream stream to read packet from
/// @param buf target buffer
/// @param maxDatagramSize maximum octet size of buf
/// @return negative on error
static int inPacket(UdpConnectionsClient* self, FldInStream* inStream, uint8_t* buf, size_t maxDatagramSize)
{
    UdpConnectionsSerializeConnectionId connectionId;
    uint16_t octetCountInPacket;

    // CLOG_C_VERBOSE(&self->log, "received packet from server")
    int err = udpConnectionsSerializeClientInPacket(inStream, &connectionId, &octetCountInPacket);
    if (err < 0) {
        return err;
    }

    if (octetCountInPacket > maxDatagramSize) {
        CLOG_SOFT_ERROR("target buffer too small")
        return -25;
    }

    if (connectionId != self->connectionId) {
        CLOG_C_SOFT_ERROR(&self->log, "wrong connectionId, probably for someone else received %" PRIX64 " vs %" PRIX64,
                          connectionId, self->connectionId)
        return -2;
    }

    if (self->phase != UdpConnectionsClientPhaseConnected) {
        CLOG_C_NOTICE(&self->log, "ignoring packet, we are not connected yet")
        return 0;
    }

    int readErr = fldInStreamReadOctets(inStream, buf, octetCountInPacket);
    if (readErr < 0) {
        return readErr;
    }

    return octetCountInPacket;
}

/// Udp Connections receive
/// Reads packet from underlying transport and checks what type of message it is.
/// @param _self udp connections client
/// @param data target octet buffer
/// @param maxOctetCount maximum octets in data
/// @return negative on error
static ssize_t transportReceive(void* _self, uint8_t* data, size_t maxOctetCount)
{
    UdpConnectionsClient* self = (UdpConnectionsClient*) _self;

    ssize_t octetsFound = datagramTransportReceive(&self->underlyingTransport, data, maxOctetCount);
    if (octetsFound <= 0) {
        return octetsFound;
    }

    if (octetsFound < 10) {
        return -1;
    }

    FldInStream inStream;
    fldInStreamInit(&inStream, data, (size_t) octetsFound);

    uint8_t cmd;
    fldInStreamReadUInt8(&inStream, &cmd);
    if (cmd != UdpConnectionsSerializeCmdPacketToClient) {
        int err = handleInStream(self, &inStream, cmd);
        if (err < 0) {
            return err;
        }
        return transportReceive(_self, data, maxOctetCount);
    }

    if (self->phase != UdpConnectionsClientPhaseConnected) {
        CLOG_C_SOFT_ERROR(&self->log, "can not receive, we are not connected yet")
        return -55;
    }

    return inPacket(self, &inStream, data, maxOctetCount);
}

/// Initializes the Udp Connections Client
/// @param self udp connections client
/// @param transport transport to use
/// @param log log target
/// @return negative on error
int udpConnectionsClientInit(UdpConnectionsClient* self, DatagramTransport transport, Clog log)
{
    self->log = log;
    self->underlyingTransport = transport;
    self->nonce = secureRandomUInt64();
    self->receivedServerChallenge = 0;
    self->phase = UdpConnectionsClientPhaseChallenge;
    self->waitTime = 0;
    self->connectionId = 0;
    self->transport.self = self;
    self->transport.send = transportSend;
    self->transport.receive = transportReceive;

    CLOG_C_DEBUG(&self->log, "randomized nonce %" PRIX64, self->nonce)
    return 0;
}

/// Sends challenge request to the server
/// @param self udp connections client
/// @param outStream stream to write challenge to
/// @return negative on error
static int outChallenge(UdpConnectionsClient* self, FldOutStream* outStream)
{
    CLOG_C_DEBUG(&self->log, "sending challenge request to server %" PRIX64, self->nonce)
    self->waitTime = 5;
    return udpConnectionsSerializeClientOutChallenge(outStream, self->nonce);
}

/// Sends connect request to server
/// @param self udp connections client
/// @param outStream write connection info to stream
/// @return negative on error
static int outConnect(UdpConnectionsClient* self, FldOutStream* outStream)
{
    CLOG_C_DEBUG(&self->log, "sending connect request to server %" PRIX64 " secretChallenge:%" PRIX64, self->nonce,
                 self->receivedServerChallenge)
    self->waitTime = 5;
    return udpConnectionsSerializeClientOutConnect(outStream, self->nonce, self->receivedServerChallenge);
}

/// Send challenge- or connect request packet to underlying transport if needed
/// @param self udp connections client
/// @return negative on error
static int sendPacket(UdpConnectionsClient* self)
{
    if (self->waitTime > 0) {
        self->waitTime--;
        return 0;
    }

#define MAX_SENDBUF_OCTET_COUNT (1200U)
    uint8_t buf[MAX_SENDBUF_OCTET_COUNT];
    FldOutStream outStream;

    fldOutStreamInit(&outStream, buf, MAX_SENDBUF_OCTET_COUNT);

    int result = 0;
    switch (self->phase) {
        case UdpConnectionsClientPhaseChallenge:
            result = outChallenge(self, &outStream);
            break;
        case UdpConnectionsClientPhaseConnect:
            result = outConnect(self, &outStream);
            break;
        case UdpConnectionsClientPhaseConnected:
            // intentionally do nothing
            break;
        case UdpConnectionsClientPhaseIdle:
            break;
    }

    if (result < 0) {
        CLOG_C_SOFT_ERROR(&self->log, "could not send")
        return result;
    }

    if (outStream.pos > 0) {
        return datagramTransportSend(&self->underlyingTransport, buf, outStream.pos);
    }

    return result;
}

/// Sends packet if needed
/// @param self udp connections client
/// @return negative on error
int udpConnectionsClientUpdate(UdpConnectionsClient* self)
{
    return sendPacket(self);
}
