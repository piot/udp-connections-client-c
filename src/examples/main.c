/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <clog/console.h>
#include <stdio.h>
#include <udp-client/udp_client.h>
#include <udp-connections-client/client.h>

#include <stdbool.h>
#include <unistd.h>

clog_config g_clog;

static int udpSend(void* _self, const uint8_t* data, size_t size)
{
    UdpClientSocket* self = _self;
    return udpClientSend(self, data, size);
}

static ssize_t udpReceive(void* _self, uint8_t* data, size_t size)
{
    UdpClientSocket* self = _self;
    return udpClientReceive(self, data, size);
}

int main(int argc, char* argv[])
{
    (void) argc;
    (void) argv;

    g_clog.log = clog_console;
    g_clog.level = CLOG_TYPE_VERBOSE;

    CLOG_VERBOSE("udp connections client example start")

    UdpClientSocket socket;

    udpClientInit(&socket, "127.0.0.1", 27000);

    DatagramTransport udpClientTransport;
    udpClientTransport.self = &socket;
    udpClientTransport.send = udpSend;
    udpClientTransport.receive = udpReceive;

    Clog log;
    log.config = &g_clog;
    log.constantPrefix = "udpClient";

    UdpConnectionsClient connectionsClient;
    udpConnectionsClientInit(&connectionsClient, udpClientTransport, log);

#define MAX_BUF_SIZE (1200)
    uint8_t buf[MAX_BUF_SIZE];
    uint32_t tickId = 0;

    while (true) {
        udpConnectionsClientUpdate(&connectionsClient);

        int receivedOctetCount = datagramTransportReceive(&connectionsClient.transport, buf, 1200);
        if (receivedOctetCount > 0) {
            CLOG_INFO("got reply: %d '%s'", receivedOctetCount, buf)
        }

        if (connectionsClient.phase == UdpConnectionsClientPhaseConnected) {
            tc_snprintf(buf, MAX_BUF_SIZE, "Hello %04X tick", tickId);
            CLOG_INFO("sending '%s' %d", buf, tc_strlen(buf) + 1)
            datagramTransportSend(&connectionsClient.transport, buf, tc_strlen(buf) + 1);
            tickId++;
        }

        usleep(16 * 1000);
    }
}
