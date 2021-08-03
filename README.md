# FoxNet

The Free Open Xross Networking library. Just another C++ networking library with an emphasis on being lightweight. FoxNet is a blocking (or non-blocking with a settable timeout!) networking library with very little overhead, it's your generic client/server model made simple.

# Compiling

```
mkdir build && cd build && cmake . ../ && make -j && cd ../
```

After compiling, run the server and client examples!

```
./bin/FoxServer & ./bin/FoxClient
```