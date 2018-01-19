# gsttransformer

A library and gRPC based service for running gstreamer based media transformation as a service.

Since gRPC clients can be generated in many programming lagnuages and platforms, the service is completely agnostic to the clients.

## Features

* #### Embeddable library, in-proc, Unix domain or TCP gRPC API

You can use `gsttransformer` has a simple library, as an in-proc gRPC in the same process, as a service over different process Unix socket, or as a service over remote TCP connections. All using simple and platform supported gRPC.

* #### Flexible media processing pipelines

Using `gst-launch` declarative media pipelines, you can either preset your pipelines or even specify them on per-call basis.

* #### Stream sanitization and format enforcement

GStreamer is a very stable and powerful media processing framework. You have all the freedom to construct your pipelines in such a way to enforce formats, do sanitation and much more.

* #### Processing flow control, duration, rate control and stream QoS

It is often the case that you want to enforce certain characteristics on how you process your media. For example, you want your pipeline to process at maximum of 1.0 Real Time speech. This allows you to manage maximum resource consumption.

`gsttransformer` allows you to control the processing rate in media time (not input bytes), and gives you the option to enforce it using blocking-based flow control or simply erroring.

## How to use

`gsttransformer` can be used in different ways:

* #### As a service ([`gst-transformer.cpp`](https://github.com/technicianted/gsttransformer/blob/master/src/server/gst-transformer.cpp))

1. Unix domain socket:
```bash
./gsttransformerserver -c config.json unix:///var/run/gsttransformer.sock
```

Docker:
```bash
docker run --rm -it \
    -v /path/to/config/:/config \
    -v /var/run/:/var/run \
    -e GSTTRANSFORMER_CONFIG_PATH=/config/server.json \
    -e GSTTRANSFORMER_ENDPOINT=unix:///var/run/gsttransformer.sock \
    technicianted/gsttransformer:experimental
```

2. TCP socket:
```bash
./gsttransformerserver -c config.json 0.0.0.0:8080
```
Docker:
```bash
docker run --rm -it \
    -v /path/to/config/:/config \
    -e GSTTRANSFORMER_CONFIG_PATH=/config/server.json \
    -e GSTTRANSFORMER_ENDPOINT=0.0.0.0:8080 \
    -p 8080:8080 technicianted/gsttransformer:experimental
```

Client:
1. Example C++ ([`gst-transformer-client.cpp`](https://github.com/technicianted/gsttransformer/blob/master/src/samples/gst-transformer-client.cpp))
```bash
# convert input video to ogg/theora at 1fps at maximum 5xrealtime rate
# connect to service at unix domain socket /var/run/gsttransformer.sock
./gsttransformerclient \
    -r 5.0
    -p video/ogg_theora_256k_1fp \
    -i la_chute_d_une_plume_576p.ogv \
    -o output.ogv \
    unix:///var/run/gsttransformer.sock
```

* #### As an in-proc gRPC for your app process ([`gst-transformer-inproc.cpp`](https://github.com/technicianted/gsttransformer/blob/master/src/samples/gst-transformer-inproc.cpp))
```bash
# convert input video to ogg/theora at 1fps at maximum 5xrealtime rate
# connect to in-proc service
./gsttransformerinproc \
    -p video/ogg_theora_256k_1fp \
    -i la_chute_d_une_plume_576p.ogv \
    -o output.ogv
```

* #### As an embedded shared library for your app process (C++ only)

In this mode, you simply link and use `gsttransformer.so` and use `DynamicPipeline` directly.

* #### Sample service configurations [`sampleconfig.json`]():
```json
{
    "limits": {
        "allowDynamicPipelines":true,
        "rate":{
            "description":"clients are request any processing rate",
            "max":-1
        },
        "lengthLimitMillis":{
            "description":"clients can process media of any length",
            "max":0
        },
        "startToleranceBytes":{
            "description":"pipeline must start before consuming first 10000 bytes",
            "max":10000
        },
        "readTimeoutMillis":{
            "description":"clients can specify read timeout of maximum 5 seconds",
            "max":5000
        },
        "pipelineOutputBuffer":{
            "description":"clients can request output buffering of up to 1MB",
            "max":1000000
        }
    },

    "pipelines": [
        {
            "id":"ogg_vorbis/pcm_16le_16khz_mono",
            "specs":"oggdemux ! vorbisdec ! audioconvert ! audioresample ! audio/x-raw,format=S16LE,channels=1,rate=16000",
            "description":"ogg/vorbis audio input to pcm16khz16le"
        },
        {
            "id":"flac/ogg_vorbis",
            "specs":"flacdec ! audioconvert ! vorbisenc ! oggmux",
            "description":"encode flac audio to ogg/vorbis"
        },
        {
            "id":"audio/pcm_16le_16khz_mono",
            "specs":"decodebin ! audioconvert ! audioresample ! audio/x-raw,format=S16LE,channels=1,rate=16000",
            "description":"normalize any audio to pcm16khz16le"
        },
        {
            "id":"video/ogg_theora_256k_1fp",
            "specs":"decodebin ! videorate ! video/x-raw,framerate=1/1 ! timeoverlay halignment=right valignment=top ! clockoverlay halignment=left valignment=top ! theoraenc bitrate=256 ! oggmux",
            "description":"normalize any video to 1fps with time, duration to ogg/theora"
        }
    ]
}
```

## Why would you need it (as a service)

If you have a service that relies or works with media, then you would face at least one of the two challenges:

#### 1. Variability and variety of media formats and codecs

Handling media processing is often challenging due to the amount of different formats, codecs and other nuiances such as timing, rate, etc. In addition, if you are an externally exposed service, you will have to deal with bad formats and misbehaving clients.

This framework provides a standalone way of deligating all this work to an separate service. All you have to do is manage the interfacing.

#### 2. Scalability

Although you can link agains `gsttransformer` library directly to embed in your apps, this creates a stong coupling between scalability of media handling and the scalability of your core service.

This is often undesirable: Typically you want service components handling different aspects to be scaled independently, especially if their resource requirements are different.

#### 3. Isolation of resources

Media handling usually involves intensive CPU and potentially memory. If your service heavily relies on media processing, then you will want to run it in isolation to be able to control its resources without affecting the main service.

## Some use-cases

* #### Media processing as a service

* #### Media normalization as a service

* #### Audio and video processing adaptors/side-cars
