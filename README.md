# FoxNet
This is a single header library for an event-driven minimal networking protocol. Written because I needed practice with sockets.

# Example

Server:
```c++
#include "foxnet.hpp"

void examplePrint(FoxNet::FoxConnection* sock, FoxNet::FoxPacket* data) {
  std::cout << (char*)data->data << std::endl;
}

int main() {
  FoxNet::init(); // sets up WSA for windows
  
  FoxNet::FoxServer server(5555); // host server on port 5555
  server.setPacket(2, examplePrint);
  
  server.start();
  
  FoxNet::cleanup(); // cleans up WSA
  return 0;
}
```

Client:
```c++
#include "foxnet.hpp"
#include <cstring>

int main() {
  FoxNet::init(); // sets up WSA
  
  FoxNet::FoxClient client("127.0.0.1", 5555); // connects to localhost on port 5555
  
  char* dataTest = "Hello world!";
  
  client.sendPacket(FoxPacket(2, (void*)dataTest, strlen(dataTest));
  
  client.kill(); // disconnects from the server
  FoxNet::cleanup(); // cleans up WSA
  return 0;
}
```
