#pragma once

#include <exception>

namespace FoxNet {
    // Generic FoxNet Exception
    struct FoxException : public std::exception {
        const char *errMsg;

        FoxException();
        FoxException(const char *errStr);
        const char *what() const throw();
    };
}