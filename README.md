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

Install `libavcodec` library (`sudo apt install libavcodec-dev` on Ubuntu).

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

See `DummyServiceConnection.cpp` for the default stream key retrieval mechanism.

For watching your stream from a browser, see [janus-ftl-player](https://github.com/Glimesh/janus-ftl-player).

# Configuration

Configuration is achieved through environment variables.
| Environment Variable   | Supported Values | Notes             |
| :--------------------- | :--------------- | :---------------- |
| `FTL_HOSTNAME`         | Valid hostname   | The hostname of the machine running the FTL service. Defaults to system hostname. |
| `FTL_SERVICE_CONNECTION` | `DUMMY`: (default) Dummy service connection <br />`GLIMESH`: Glimesh service connection | This configuration value determines which service FTL should plug into for operations such as stream key retrieval. |
| `FTL_SERVICE_METADATAREPORTINTERVALMS` | Time in milliseconds | Defaults to `4000`, controls how often FTL stream metadata will be reported to the service. |
| `FTL_SERVICE_DUMMY_PREVIEWIMAGEPATH` | `/path/to/directory` | The path where preview images of ingested streams will be stored if service connection is `DUMMY`. Defaults to `~/.ftl/previews` |
| `FTL_SERVICE_GLIMESH_HOSTNAME` | Hostname value (ex. `localhost`, `glimesh.tv`) | This is the hostname the Glimesh service connection will attempt to reach. |
| `FTL_SERVICE_GLIMESH_PORT` | Port number, `1`-`65535`. | This is the port used to communicate with the Glimesh service via HTTP/HTTPS. |
| `FTL_SERVICE_GLIMESH_HTTPS` | `0`: Use HTTP <br />`1`: Use HTTPS | Determines whether HTTPS is used to communicate with the Glimesh service. |
| `FTL_SERVICE_GLIMESH_CLIENTID` | String, OAuth Client ID returned by Glimesh app registration. | Used to authenticate to the Glimesh API. |
| `FTL_SERVICE_GLIMESH_CLIENTSECRET` | String, OAuth Client Secret returned by Glimesh app registration. | Used to authenticate to the Glimesh API. |

# Dockering

    docker build -t janus-ftl
    docker run --rm -p 8084:8084/tcp -p 8088:8088/tcp -p 9000-9100:9000-9100/udp -p 20000-20100:20000-20100/udp -e "DOCKER_IP=HOST.IP.ADDRESS.HERE" janus-ftl

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
