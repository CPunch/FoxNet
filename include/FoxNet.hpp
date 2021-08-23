#pragma once

#include <iostream>
#include <cstring>

#define FOXFATAL(x) \
    std::cerr << "[FATAL] FoxNet: " << x << std::endl; \
    exit(EXIT_FAILURE);

#define FOXWARN(x) \
    std::cerr << "[WARN] FoxNet: " << x << std::endl; \

// MINOR defines a bugfix release or other misc. changes that shouldn't have an effect on the protocol
#define FOXNET_MINOR 1

// MAJOR defines a protocol version
#define FOXNET_MAJOR 0

#define FOXMAGIC "FOX\71"
#define FOXMAGICLEN strlen(FOXMAGIC)