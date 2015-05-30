flux
====

Middlelayer for controlling lux devices &amp; more: "jackd for lights"


Building
========

Flux is based on [ZMQ](http://zeromq.org/) and uses the [CZMQ](http://czmq.zeromq.org/) library. 
CZMQ is not commonly available as a prepackaged binary, so it is reccomended to build it from source:
```
git submodule update --init
cd libsodium
./autogen.sh
./configure && make check
sudo make install
sudo ldconfig
cd ..

cd libzmq
./autogen.sh
./configure && make check
sudo make install
sudo ldconfig
cd ..

cd czmq
./autogen.sh
./configure && make check
sudo make install
sudo ldconfig
cd ..
```

Once you have CZMQ, you can build flux:
```
make
sudo make install
```

Model
=====
Flux uses a three-part model: the *broker*, *clients*, and *servers*. 

- **Broker**: There needs to be a single *broker* instance per flux system. The broker can be started with `flux-broker`. Both the clients and servers connect to the broker, who routes messages from clients to the servers that expose the requested devices. 

- **Server**: Each server process can expose one or more *devices* to be controlled, each with a global, semantic, and unique ID (i.e. `"leds:00000001"`).

- **Client**: Each client process can control any of the devices exposed by servers that the broker knows about. 

These processes communicate over ZMQ (usually TCP sockets), so they can exist on separate machines. Fundamentally, the client is agnostic to which devices are connected to which servers. 

The broker is designed around the [Majordomo](http://rfc.zeromq.org/spec:7) protocol, with some modifications made to allow multiple "workers" (devices) per server. In the future the broker may also allow clients to "own" devices to prevent other clients from trying to control them at the same time.

Messages
--------

### Flux API

Client-server communications follows the request-reply model: servers cannot initiate contact with clients. Each client message results in exactly one message from a server, or no response (error).

Client messages to the server (requests) consist of a ``destination``, ``command``, and ``body``.

- ``char * destination``: a string corresponding to a device ID. (``flux_id_t`` is typedef'd to ``char[16]`` and may be used instead)
- ``char * command``: a string corresponding to a device command. Usually all caps, e.g. ``"ECHO"``.
- ``zmsg_t * body``: a zmsg which can be used to pass "arguments" to the given device command. Can be an empty message.

Server messages to the client (replies) only consist of a ``body`` and an error code.

#### Usage

The Flux C API exposes methods for handling messages under this request-reply model:

##### Client

On the client side, the primary method ``client_send`` synchronously sends a message and waits for a reply (or timeout/error).

``int client_send(client_t * client, char * destination, char * command, zmsg_t ** body, zmsg_t ** reply)``

The ``body`` parameter is passed by reference because ``client_send`` takes ownership of it and destroys it. Inversely, ``reply`` should be a reference to a ``NULL`` pointer that will be populated with the response, which you then need to destroy after use.

The return value is ``0`` on success, and ``-1`` on failure (timeout, destination does not exist, "500" response from server, etc.). If the function is successful, the reply is stored in the ``*reply`` variable. 

##### Server

On the server side, each ``resource_t`` (which corresponds to a device) has a ``request`` function with the following type (``request_fn_t``):

``int request(void * args, const char * command, zmsg_t * body, zmsg_t ** response)``

When the server receives a message for a given device, this method is called. ``args`` is populated from ``resource_t`` and can be used to pass device-specific information. 

Do not attempt to free/destroy ``body``.

To indicate success, populate ``response`` with a new (possibly empty) zmsg and return ``0``. The calling context will destory the zmsg after sending it.

To indicate failure, return ``-1``. You can optionally populate ``response`` if you want to send additional information about the error to the client (e.g. an error message).

### Common Commands

The following commands should be implemented for all devices on the server.
- ``PING`` no body, replies with a single frame consisting of ``"PONG"``.
- ``INFO`` no body, replies with a single frame with a serialized zhash containing device information.

Usage
=====

- Start the broker: `flux-broker tcp://*:5555`
- Start the server(s): `./flux-server tcp://localhost:5555`
- Start the client(s): `./flux-client tcp://localhost:5555`

(In general, these can start in any order.)

