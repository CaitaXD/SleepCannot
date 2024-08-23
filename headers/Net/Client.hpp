#ifndef CLIENT_H_
#define CLIENT_H_

#include "Socket.hpp"
#include "../DataStructures/Result.hpp"

struct TcpClient
{
    static Result<TcpClient> connect(AddressFamily family, SocketType type, SocketProtocol protocol, const IpEndpoint &endpoint);
    Result<Socket> connect();
    Socket socket = {};

private:
    AddressFamily _family = {};
    SocketType _type = {};
    IpEndpoint _endpoint = {};
    SocketProtocol _protocol = {};
};

#define CLIENT_IMPLEMENTATION
#ifdef CLIENT_IMPLEMENTATION

Result<TcpClient> TcpClient::connect(AddressFamily family, SocketType type, SocketProtocol protocol, const IpEndpoint& endpoint)
{
    Result<TcpClient> result;
    TcpClient client;
    client._family = family;
    client._endpoint = endpoint;
    client._type = type;
    client._protocol = protocol;
    int r = client.socket.open(family, type, protocol);
    Socket client_socket = client.socket.connect(endpoint);
    if (r < 0)
    {
        client.socket.close();
        result.emplace(client_socket.lasterrno);
    }
    result.emplace(std::move(client));
    return result;
}

#endif // CLIENT_IMPLEMENTATION
#endif // CLIENT_H_