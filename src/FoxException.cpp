#include "FoxException.hpp"

FoxNet::FoxException::FoxException() {
    errMsg = "N/A";
}

FoxNet::FoxException::FoxException(const char *str) {
    errMsg = str;
}

const char *FoxNet::FoxException::what() const throw() {
    return errMsg;
}