# Streams

# Source

A source is something which generates data.

Sources are connected to zero or more sinks. Each time a source has data, it will emit that
data to all sinks. Sinks must acknowledge that item before a new item can be emitted. If no
sinks are attached, the item emitted from the source is lost.

A source can implement queuing and backpressure.

## Existing sources

### ASIO

* TCP socket - this emits bytes as we receive data
* TCP listener - this emits a stream whenever there's an incoming connection
* UDP socket - this emits messages on incoming packets
* File - pulls data from an open filehandle

# Sink

A sink receives data.

## Existing sinks

### ASIO

* TCP socket - this accepts bytes as outgoing data
* UDP socket - this accepts message packets and sends them out
* File - will write bytes to a filehandle

