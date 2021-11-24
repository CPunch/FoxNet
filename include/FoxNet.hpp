#pragma once

#include <iostream>
#include <cstring>

#include "FoxException.hpp"

#define FOXFATAL(x) \
    throw FoxNet::FoxException(x);

#define FOXWARN(x) \
    std::cerr << "[WARN] FoxNet: " << x << std::endl; \

#ifdef _FOXNET_VER_MAJOR
// MAJOR defines a protocol version
#define FOXNET_MAJOR _FOXNET_VER_MAJOR

// MINOR defines a bugfix release or other misc. changes that shouldn't have an effect on the protocol
#define FOXNET_MINOR _FOXNET_VER_MINOR
#else // fixes intellisense
#define FOXNET_MINOR 0
#define FOXNET_MAJOR 0
#endif

#define FOXMAGIC "FOX\71"
#define FOXMAGICLEN 4