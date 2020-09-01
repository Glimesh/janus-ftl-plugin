# Janus FTL Plugin

This is a work-in-progress [Janus](https://github.com/meetecho/janus-gateway) plugin
to support the FTL "Faster-Than-Light" protocol developed for the Mixer live streaming service.

This protocol lets streamers deliver media to browser viewers with very low,
often sub-second latency.

See my notes on the FTL protocol [here](https://hayden.fyi/posts/2020-08-03-Faster-Than-Light-protocol-engineering-notes.html).

# Building

## Dependencies

First, compile and install [Janus](https://github.com/meetecho/janus-gateway).

Get [Meson](https://mesonbuild.com/Getting-meson.html) for building.

## Building

By default during build we look for Janus in `/opt/janus` (the default install path), but this can be configured with the `JANUS_PATH` env var.

```sh
mkdir build/
meson build/
cd build
ninja
```

## Installing

_(from `build/` directory)_

```sh
sudo ninja install
```

## Running

Just fire up Janus (`/opt/janus/bin/janus`), and the FTL plugin should automatically load - you should see this output:

```log
FTL: Plugin initialized!
[...]
FTL: Ingest server is listening on port 8084
```

Now you ought to be able to point an FTL client at the ingest server and start streaming.

The default stream key is `123456789-aBcDeFgHiJkLmNoPqRsTuVwXyZ123456`.

`123456789` can be whatever "Channel ID" you'd like.

See `DummyCredStore.cpp` for the default stream key retrieval mechanism.

For watching your stream from a browser, see [janus-ftl-player](https://github.com/haydenmc/janus-ftl-player).

# Dockering

    docker build -t janus-ftl
    docker run -p 8088:8088 -p 8089:8089 -p 8084:8084 -p 9000-10000:9000-10000/udp janus-ftl

# Misc Notes

## Include paths

If you use VS code with the C++ extension, these include paths should make intellisense happy.

```json
"includePath": [
    "${workspaceFolder}/**",
    "/opt/janus/include/janus",
    "/usr/include/glib-2.0",
    "/usr/lib/x86_64-linux-gnu/glib-2.0/include"
]
```