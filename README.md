# chat-server
Basic chat server implementation, allowing for communication between users connected to the same port on the chat server.

## Usage:
With Make installed, user simply needs to type `make` in their system shell to generate both client and server side executables. This will require that `make` and `gcc` are both installed on your system.

### Starting chat server
To start the server, simply run 
```bash
./chat-server port_number
```

where `port_number` is a number from 1024 - 49151 corresponding to the available user ports. You will know that this program has run successfully because the server-side console will print out a message confirming the port on which the program is listening. To end the server, kill the program with `CTRL + C` and the connected clients will be notified and disconnected.

### Connecting as a client
To connect as a client, you need to know the hostname and socket to connect to. For the simplest example, you can connect to a chat server on your own device by specifying the hostname as `localhost` or `0.0.0.0` and connecting on the same `port_number` as before.

```bash
./chat-client localhost port_number
```

You will see a `Connected` message upon connecting to the server, from which point you can send messages which will be sent to all other connected clients.

Currently, there is a `/nick` feature which allows a connected user to change their display name, which is otherwise `unknown` by default. 

To disconnect, clients can press `CTRL + D` or type `/exit`, though functionality of the latter is still being improved.
