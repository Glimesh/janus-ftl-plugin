# Janus FTL Plugin

This is a work-in-progress [Janus](https://github.com/meetecho/janus-gateway) plugin
to support the FTL "Faster-Than-Light" protocol developed for the Mixer live streaming service.

This protocol lets streamers deliver media to browser viewers with very low,
often sub-second latency.

See my notes on the FTL protocol [here](https://hayden.fyi/posts/2020-08-03-Faster-Than-Light-protocol-engineering-notes.html).

# Misc notes for me

## Doc stuff

https://janus.conf.meetecho.com/docs/plugin_8h.html

## IDE/Build stuff...

```
    "includePath": [
        "${workspaceFolder}/**",
        "/opt/janus/include/janus",
        "/usr/include/glib-2.0",
        "/usr/lib/x86_64-linux-gnu/glib-2.0/include"
    ],
    "defines": [
        "HAVE_SRTP_2=1"
    ],
```

## FTL Janus API

### Watch request
```json
{
    request: "watch",
    channelId: 123456789
}
```