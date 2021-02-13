# REST Service Connection
## Details
Using the `REST` service connection mode you can plug Janus FTL into your own service by exposing a compatible HTTP API. See the project's [README](../README.md#configuration) for documentation on how to set up Janus FTL to use the REST service.

Both request and response payloads are encoded using JSON, with the exception of `/preview/{streamId}`, which uses form multipart encoding for uploading the preview JPEG. Request payloads and expected responses are represented as simplified JSON here for the sake of brevity.

## API Authorization
If the `FTL_SERVICE_REST_AUTH_TOKEN` environment variable is set, all API requests will be sent with the `Authorization` header set to the value set by the environment variable.

## Routes
| Path | Method | Payload | Expected Response |
| - | - | - | - |
| `/hmac/{channelId}` | `GET` | N/A | `{ hmacKey: string }` |
| `/start/{channelId}` | `POST` | N/A | `{ streamId: string }` |
| `/metadata/{streamId}` | `POST` | [See Below](#metadata-payload) | Any 2xx response |
| `/end/{streamId}` | `POST` | N/A | Any 2xx response |
| `/preview/{streamId}` | `POST` | [See Below](#preview-payload) | Any 2xx response |

### Metadata Payload
```
{
    audioCodec: string
    ingestServer: string
    ingestViewers: number
    lostPackets: number
    nackPackets: number
    recvPackets: number
    sourceBitrate: number
    sourcePing: number
    streamTimeSeconds: number
    vendorName: string
    vendorVersion: string
    videoCodec: string
    videoHeight: number
    videoWidth: number
}
```

### Preview Payload
The preview payload is encoded as a multipart form, with a single key `thumbdata` containing the stream's current preview JPEG.

## Channel ID Authorization
If you wish to restrict what channel IDs can start streaming, you can respond to `/hmac/{channelId}` or `/start/{channelId}` with a 4xx status code.
