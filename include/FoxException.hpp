#pragma once

#include <exception>

namespace FoxNet {
    // Generic FoxNet Exception
    struct FoxException : public std::exception {
        const char *errMsg;

        FoxException(void);
        FoxException(const char *errStr);
        const char *what(void) const throw();
    };
}