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

### Use GCC 10 compiler

This project utilizes some C++20 features available only in GCC 10 or newer.

If you are building on a recent Ubuntu distribution, you can install the `gcc-10` and `g++-10` packages and configure meson to use them for compilation:

```sh
CC=gcc-10 CXX=g++-10 meson build/
```

### Building for production

To enable optimizations, set meson to build in `debugoptimized` mode (recommended instead of `release` so you can use the debug information diagnose issues).

```sh
meson --buildtype=debugoptimized build/
```

If you've already previously configured meson, you can reconfigure it:

```sh
meson --reconfigure --buildtype=debugoptimized build/
```

### Building on resource-constrained machines

Some machines (like the teensy tiny DigitalOcean droplet) will fail to finish building with the default configuration.

Consider configuring ninja to disable parallel builds to allow the build to finish successfully:

```sh
cd build
ninja -j 1
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

`aBcDeFgHiJkLmNoPqRsTuVwXyZ123456` can be overridden by setting the `FTL_SERVICE_DUMMY_HMAC_KEY` environment variable.

See `DummyServiceConnection.cpp` for the default stream key retrieval mechanism.

For watching your stream from a browser, see [janus-ftl-player](https://github.com/Glimesh/janus-ftl-player).

# Configuration

Configuration is achieved through environment variables.
| Environment Variable   | Supported Values | Notes             |
| :--------------------- | :--------------- | :---------------- |
| `FTL_HOSTNAME`         | Valid hostname   | The hostname of the machine running the FTL service. Defaults to system hostname. |
| `FTL_NODE_KIND`        | `Standalone`: (default) This instance will listen for incoming FTL connections and transmit them to WebRTC clients.<br />`Ingest`: This instance will listen for incoming FTL connections and relay them to other nodes when instructed by an Orchestrator service.<br />`Edge`: This instance will receive stream relays from other nodes and transmit them to WebRTC clients. | This configuration value controls the behavior of the FTL plugin when used in conjunction with an [Orchestrator service](https://github.com/Glimesh/janus-ftl-orchestrator). |
| `FTL_ORCHESTRATOR_HOSTNAME` | Valid hostname | The hostname of the Orchestrator service to connect to for stream relay information. |
| `FTL_ORCHESTRATOR_PORT` | Port number, `1`-`65535`. | The port number to use when connecting to the Orchestrator service. |
| `FTL_ORCHESTRATOR_PSK` | String of arbitrary hex values (ex. `001122334455ff`) | This is the pre-shared key used to establish a secure TLS1.3 connection to the Orchestrator service. |
| `FTL_ORCHESTRATOR_REGION_CODE` | String value, default: `global` | This is a string value used by the Orchestrator to group regional nodes together to more effectively distribute video traffic. |
| `FTL_SERVICE_CONNECTION` | `DUMMY`: (default) Dummy service connection <br />`GLIMESH`: Glimesh service connection <br />`REST`: REST service connection ([docs](docs/REST_SERVICE.md)) | This configuration value determines which service FTL should plug into for operations such as stream key retrieval. |
| `FTL_SERVICE_METADATAREPORTINTERVALMS` | Time in milliseconds | Defaults to `4000`, controls how often FTL stream metadata will be reported to the service. |
| `FTL_MAX_ALLOWED_BITS_PER_SECOND` | Integer bits per second | Defaults to `0` (disabled), FTL connections that exceed the bandwidth specified here will be stopped.<br />**Note that this is a strictly enforced maximum** based on a rolling average; consider providing some buffer size for encoder spikes above the configured average. |
| `FTL_SERVICE_DUMMY_HMAC_KEY` | String, default: `aBcDeFgHiJkLmNoPqRsTuVwXyZ123456` | Key all FTL clients must use if service connection is `DUMMY`. The HMAC key is the part after the dash in a stream key. |
| `FTL_SERVICE_DUMMY_PREVIEWIMAGEPATH` | `/path/to/directory` | The path where preview images of ingested streams will be stored if service connection is `DUMMY`. Defaults to `~/.ftl/previews` |
| `FTL_SERVICE_GLIMESH_HOSTNAME` | Hostname value (ex. `localhost`, `glimesh.tv`) | This is the hostname the Glimesh service connection will attempt to reach. |
| `FTL_SERVICE_GLIMESH_PORT` | Port number, `1`-`65535`. | This is the port used to communicate with the Glimesh service via HTTP/HTTPS. |
| `FTL_SERVICE_GLIMESH_HTTPS` | `0`: Use HTTP <br />`1`: Use HTTPS | Determines whether HTTPS is used to communicate with the Glimesh service. |
| `FTL_SERVICE_GLIMESH_CLIENTID` | String, OAuth Client ID returned by Glimesh app registration. | Used to authenticate to the Glimesh API. |
| `FTL_SERVICE_GLIMESH_CLIENTSECRET` | String, OAuth Client Secret returned by Glimesh app registration. | Used to authenticate to the Glimesh API. |
| `FTL_SERVICE_REST_HOSTNAME` | Hostname value (ex. `localhost`) | This is the hostname the REST service connection will attempt to reach. |
| `FTL_SERVICE_REST_PORT` | Port number, `1`-`65535`. | This is the port used to communicate with the REST service via HTTP/HTTPS. |
| `FTL_SERVICE_REST_HTTPS` | `0`: Use HTTP <br />`1`: Use HTTPS | Determines whether HTTPS is used to communicate with the REST service. |
| `FTL_SERVICE_REST_PATH_BASE` | String, default: `/` | Used to add a path prefix to all REST API calls. |
| `FTL_SERVICE_REST_AUTH_TOKEN` | String, default: `""` | Used to authenticate REST service API calls using the `Authorization` header. Leave blank to disable. |

# Dockering

    docker build -t janus-ftl .
    docker run --rm -p 8084:8084/tcp -p 8088:8088/tcp -p 9000-9100:9000-9100/udp -p 20000-20100:20000-20100/udp -e "DOCKER_IP=HOST.IP.ADDRESS.HERE" janus-ftl

# Misc Notes

## Streaming from OBS

Currently, there is no UI in OBS to set a custom FTL ingest endpoint.

In order to specify a custom FTL ingest server, you will need to edit the `plugin_config\rtmp-services\services.json` file. This is located in `%AppData%\obs-studio\plugin_config\rtmp-services\services.json` on Windows.

Add the following to the `"services"` array:

```json
{
    "name": "YOUR NAME HERE",
    "common": false,
    "servers": [
        {
            "name": "SERVER NAME HERE",
            "url": "your.janus.hostname"
        }
    ],
    "recommended": {
        "keyint": 2,
        "output": "ftl_output",
        "max audio bitrate": 160,
        "max video bitrate": 8000,
        "profile": "main",
        "bframes": 0
    }
},
```

After you've made this change, you can start OBS and find your service listed in the OBS settings UI.

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

# License

```
Copyright (C) 2020  Hayden McAfee

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```
