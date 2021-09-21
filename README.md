# RTMP Restreamer
Portable RTMP restreaming tool. Starts server, accepts any stream and restreams it to multiple destinations without reencoding.

## Usage
To get help:
```C
rtmp_restreamer --help
```

## Config file
Program searches config file *config.json* in executable directory, but you can
set path with -c flag. Config file contains routing matrix of restreaming. Server
accepts many streams and restreams every accepted stream to multiple destinations.
This example set up that stream with key *restream* to be restreamed to Facebook,
YouTube, Twitch, and stream with key *restream2* to custom server. *key* is
streaming key. Note, to use RTMPS, program must be built with OpenSSL.
```json
{
    "restream": [
        "rtmps://live-api-s.facebook.com:443/rtmp/key",
        "rtmp://a.rtmp.youtube.com/live2/key"
        "rtmp://waw.contribute.live-video.net/app/key"
    ],
    "restream2":[
        "rtmps://example.com/app/key"
    ]
}
```
Point your encoder to *rtmp://{host}/app/restream* where host is IP address or
DNS name of PC where program is running. Messages about restreaming should appear.

## Dependencies
* C++11 compiler (tested on MSVCv14.29, gcc 8.3.0)
* [EasyRTMP](https://github.com/AlexanderBabansky/EasyRTMP)
* OpenSSL (optional, to support RTMPS)

## Building 
Project has CMake build script

## Acknowledgements
* [cxxopts](https://github.com/jarro2783/cxxopts) by jarro2783
* [json](https://github.com/nlohmann/json) by nlohmann
