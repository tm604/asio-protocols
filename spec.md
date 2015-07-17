# HTTP

The HTTP protocol provides HTTP/1.1, SPDY/3.1 and HTTP/2.0.


# Statsd

Request types:

* Gauge
* Timing
* Inc/dec

The server stores details in a hierarchy.

Querying a specific key will return the current value. You can also retrieve
all subkeys under a specific key.

# Abstraction

## TCP

This is a stream-based protocol.

### Server

A server is implemented as an acceptor, which is given host/port details, and will
generate streams on client connections.

### Client

A client will connect to an endpoint and return a stream.

## net::protocol::stream

This provides a streaming interface with internal read/write buffers.

The transport layer is implemented separately.

## UDP

This is a packet-based protocol. Outgoing traffic consists of a queue of variable-length messages.
Destinations may vary for outgoing messages.

