# FoxNet

<p align="center">
    <a href="https://github.com/CPunch/FoxNet/actions/workflows/check-build.yaml"><img src="https://github.com/CPunch/FoxNet/actions/workflows/check-build.yaml/badge.svg" alt="Workflow"></a>
    <a href="https://discord.gg/jPKQ3X2"><img src="https://img.shields.io/badge/chat-on%20discord-7289da.svg?logo=discord" alt="Discord"></a>
    <a href="https://github.com/CPunch/FoxNet/blob/master/LICENSE.md"><img src="https://img.shields.io/github/license/CPunch/FoxNet" alt="License"></a>
</p>

Just another C++ networking library with an emphasis on being lightweight & portable. FoxNet is a blocking (or non-blocking with a settable timeout!) networking library with very little overhead, it's your generic client/server model made simple.

## Features

- Cross-platform polling interface (epoll on Linux, poll on the other platforms)
- Support for both variable-length packets and static length packets.
- Easy to use method-based event callbacks. Just define your own FoxPeer/FoxServerPeer class (see `examples/`)

## Compiling

```
cmake -B build && cmake --build build
```

After compiling, the example binaries will be in the `bin/` directory. 

## Documentation

Documentation is pending, stay tuned!