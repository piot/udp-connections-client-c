# UDP Connections Client

A very thin protocol to establish "connections" for unreliable datagrams.

> :warning: This library is used for testing purposes, do not use in production!

Will be renamed to Datagram Connection Client or similar, since it does not have any dependencies against UDP/IP.

## Usage

### Initialize

Uses the provided UdpTransportInOut for actual sending and receiving the datagrams. It provides the field `UdpTransportInOut transport` that should be used for communicating over the this protocol.

```c
int udpConnectionsClientInit(UdpConnectionsClient* self, UdpTransportInOut transport, Clog log);
```

### Update

The udpConnectionsClientUpdate will send packets to establish a connection.

```c
int udpConnectionsClientUpdate(UdpConnectionsClient* self);
```
