flux
====

Middlelayer for controlling lux devices &amp; more: "jackd for lights".

Model
=====

### Project Goals

Flux is intended to connect user interfaces for running visual displays to the physical hardware that controls the lights and other effects.It needs to be low-latency (millisecond) while carrying enough bandwidth for raw (low-resolution) video data.

You should use flux if you're building one (or both!) of the following:

- **Visual devices** such as LED strips, large LED arrays, spotlights, etc. that are intended to be controlled from a computer.
- **User interfaces** for controlling said displays ("LJ software").

The intent is to add some interoperability between the two layers.

### Design

Flux uses a three-part model: the *broker*, *clients*, and *servers*. 

- **Broker**: There needs to be a single *broker* instance per flux system. The broker can be started with `flux-broker`. Both the clients and servers connect to the broker, who routes messages from clients to the servers that expose the requested devices. 

- **Server**: Each server process can expose one or more *devices* to be controlled, each with a global, semantic, and unique ID (e.g. `"leds:00000001"`).

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

On the client side, the primary method ``flux_cli_send`` synchronously sends a message and waits for a reply (or timeout/error).

``int flux_cli_send(flux_cli_t * client, const char * destination, const char * command, zmsg_t ** body, zmsg_t ** reply)``

The ``body`` parameter is passed by reference because ``flux_cli_send`` takes ownership of it and destroys it. Inversely, ``reply`` should be a reference to a ``NULL`` pointer that will be populated with the response, which you then need to destroy after use.

The return value is ``0`` on success, and ``-1`` on failure (timeout, destination does not exist, "500" response from server, etc.). If the function is successful, the reply is stored in the ``*reply`` variable. 

##### Server

On the server side, each ``flux_dev_t`` (which corresponds to a device) has a ``request`` function with the following type (``flux_request_fn_t``):

``int request(void * args, const char * command, zmsg_t ** body, zmsg_t ** reply)``

When the server receives a message for a given device, this method is called. ``args`` is populated from ``flux_dev_t`` and can be used to pass device-specific information. 

The ownership of ``body`` is passed onto `request`. Either you should call `zmsg_destroy(body)` before returning, or you need to pass on ownership of `body` to someone else (i.e. by setting `*reply = *body` as in `ECHO`).

To indicate success, populate ``reply`` with a new (possibly empty) zmsg and return ``0``. The calling context will destory the zmsg after sending it.

To indicate failure, return ``-1``. You can optionally populate ``reply`` if you want to send additional information about the error to the client (e.g. an error message).

### Common Commands

The following commands should be implemented for all devices on the server.
- ``PING`` no body, replies with a single frame consisting of ``"PONG"``.
- ``INFO`` no body, replies with a single frame with a serialized zhash containing device information.

Example Usage
=============

- Start the broker: `flux-broker "tcp://*:1365" -v`
- Start the server(s): `./flux-server -b "tcp://localhost:1365" -d "dummy:0" -v`
- Start the client(s): `./flux-client "tcp://localhost:1365" -v`

(In general, these can start in any order.)

#### flux-server
`./flux-server` is a [lux](http://github.com/ervanalb/lux) bridge. 

It looks for a serial port in the form `/dev/ttyUSBx`. Before running `./flux-server`, you need to run `python setup_serial.py` to configure the serial port with the correct settings (setting the speed to 3 megabaud, which I can't figure out how to do from C correctly). You may also need to `sudo chown $USER /dev/ttyUSB*`. 

It takes in an optional command line argument, a dummy ID (i.e. `-d "dummy:0"`). If this argument is passed in, then the server will create a mock device that will respond to `ECHO` `PING` and `INFO` commands but will be otherwise uninteresting. 

Due to hardware issues (unreliable receive), the server can operate in *write-only* mode by passing in the `-r` flag. In  this mode, no data is read over the lux bus, and frames are sent blindly to all configured addresses.

#### flux-client
`flux-client` is an example client which sends some `ECHO` and `INFO` commands to available devices for debugging. 

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

You can also take a shortcut by starting with ZMQ if you can get it:
```
sudo apt-get install libsodium-dev libzmq3-dev libtool

git submodule update --init
cd czmq
./autogen.sh
./configure && make check
sudo make install
sudo ldconfig
cd ..

make
sudo make install
```
