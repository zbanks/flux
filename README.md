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

- **Broker**: There needs to be a single *broker* instance per flux system. The broker can be started with `flux-broker`. Both the clients and servers connect to the broker, who routinely checks to see which devices are available.

- **Server**: Each server process can expose one or more *devices* to be controlled, each with a global, semantic, and unique ID (e.g. `"leds:00000001"` with type `lux_id_t`).

- **Client**: Each client process can control any of the devices exposed by servers that the broker knows about. 

These processes communicate using [nanomsg](http://nanomsg.org/) (usually TCP sockets), so they can exist on separate machines. Fundamentally, the client is agnostic to which devices are connected to which servers. 

Messages
--------

### Flux API

Client-server communications follows the request-reply model: servers cannot initiate contact with clients. Each client message results in exactly one message from a server, or no response (error).

Client messages to the server (requests) consist of a ``destination``, ``command``, ``body``, and ``body_size``.

- ``flux_id_t destination``: a string corresponding to a device ID. (``flux_id_t`` is typedef'd to ``char[16]``)
- ``flux_cmd_t command``: a string corresponding to a device command. Usually all caps, e.g. ``"ECHO"``. (``flux_cmd_t`` is typedef'd to ``char[16]``)
- ``char * body``: an array of data which can be used to pass "arguments" to the given device command. Can be an empty message.
- ``size_t body_size``: length of the ``body`` array.

Server messages to the client (replies) only consist of a ``char * reply`` and its corresponding length.

#### Usage

The Flux C API exposes methods for handling messages under this request-reply model:

##### Client

On the client side, the primary method ``flux_cli_send`` synchronously sends a message and waits for a reply (or timeout/error).

``int flux_cli_send(flux_cli_t * client, const flux_id_t destination, const flux_cmd_t command, const char * body, size_t body_size, char ** reply)``

``reply`` should be a reference to a ``NULL`` pointer that will be populated with the response, which you then need to destroy after use.

The return value is ``-1`` on failure (timeout, destination does not exist, "500" response from server, etc.). If the function is successful, the reply is stored in the ``*reply`` variable and the length of the reply is returned.

##### Server

On the server side, each ``flux_dev_t`` (which corresponds to a device) has a ``request`` function with the following type (``flux_request_fn_t``):

``int request(void * args, const lux_cmd_t command, char * body, size_t body_size, char ** reply)``

When the server receives a message for a given device, this method is called. ``args`` is populated from ``flux_dev_t`` and can be used to pass device-specific information. 

Ownership here is weird: request does not own `body`, but it owns whatever it populates `reply` with. (This is considered a bug and will likely be refactored)

To indicate success, populate ``reply`` with a new (possibly empty) pointer and return the length of the reply. (The calling context will destory `body` after sending `*reply`, which allows you to implement echo without copy: `*reply = body; return body_size;`)

To indicate failure, return ``-1``. You can optionally populate ``reply`` if you want to send additional information about the error to the client (e.g. an error message).

### Common Commands

The following commands should be implemented for all devices on the server.
- ``PING`` no body, replies with a single frame consisting of ``"PONG"``.

Example Usage
=============

- Start the broker: `flux-broker -s 'tcp://*:1366' -c 'tcp://*:1365' -v`
- Start the server(s): `./flux-server -b "tcp://localhost:1366" -s 'tcp://localhost:1388' -d "dummy:0" -v`
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

Flux is based on [nanomsg](http://nanomsg.org/). You can compile it from source as follows:
```
git clone git@github.com:nanomsg/nanomsg.git
cd nanomsg
./autogen.sh
./configure
make
make check
sudo make install
```

You can also installing it from your package manager. On Ubuntu:
```
sudo apt-get install libnanomsg-dev
```

Once you have nanomsg, you can build flux:
```
git clone git@github.com:zbanks/flux.git
cd flux
make
sudo make install
```
