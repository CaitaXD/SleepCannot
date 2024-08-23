#ifndef SERVER_H_
#define SERVER_H_

#include "Socket.hpp"
#include "../DataStructures/Result.hpp"

struct SocketServer
{
    static Result<SocketServer> serve(AddressFamily family, SocketType type, SocketProtocol protocol, IpEndpoint endpoint, int backlog);

    Result<Socket> bind(AddressFamily family, SocketType type, SocketProtocol protocol, IpEndpoint endpoint);
    Result<Socket> listen(int backlog);
    Result<Socket> accept(IpEndpoint &endpoint);
    Result<Socket> close();

    Socket socket = {};
    IpEndpoint endpoint = {};

private:
    AddressFamily _family = {};
    SocketType _type = {};
    SocketProtocol _protocol = {};
    int _backlog = {};
};

#define SERVER_IMPLEMENTATION
#ifdef SERVER_IMPLEMENTATION

Result<SocketServer> SocketServer::serve(AddressFamily family, SocketType type, SocketProtocol protocol, IpEndpoint endpoint, int backlog)
{
    Result<SocketServer> result;
    Socket socket = {};
    int r = socket.open(family, type, protocol);
    r |= socket.bind(endpoint);
    r |= socket.listen(backlog);
    if (r < 0)
    {
        socket.close();
        result.emplace(socket.lasterrno);
        return result;
    }
    SocketServer server;
    server.socket = std::move(socket);
    server._family = family;
    server.endpoint = endpoint;
    server._protocol = protocol;
    server._backlog = backlog;
    result.emplace(std::move(server));
    return result;
}

Result<Socket> SocketServer::accept(IpEndpoint &endpoint)
{
    Result<Socket> result;
    Socket client_socket = socket.accept(endpoint);
    int r = client_socket.file_descriptor;

    if (r < 0 && client_socket.lasterrno != 0)
    {
        result.emplace(client_socket.lasterrno);
        return result;
    }
    result.emplace(std::move(client_socket));
    return result;
}

Result<Socket> SocketServer::listen(int backlog)
{
    Result<Socket> result;
    int r = socket.listen(backlog);
    if (r < 0)
    {
        result.emplace(socket.lasterrno);
        return result;
    }
    _backlog = backlog;
    result.emplace(std::move(socket));
    return result;
}

Result<Socket> SocketServer::close()
{
    Result<Socket> result;
    int r = socket.close();
    if (r < 0)
    {
        result.emplace(socket.lasterrno);
        return result;
    }
    result.emplace(std::move(socket));
    return result;
}

Result<Socket> SocketServer::bind(AddressFamily family, SocketType type, SocketProtocol protocol, IpEndpoint endpoint)
{
    Result<Socket> result;
    Socket socket = {};
    if (socket.open(family, type, protocol) < 0)
    {
        result.emplace(socket.lasterrno);
        return result;
    }
    int r = socket.bind(endpoint);
    if (r < 0)
    {
        result.emplace(socket.lasterrno);
        return result;
    }
    result.emplace(std::move(socket));
    return result;
}

#endif // SERVER_IMPLEMENTATION
#endif // SERVER_H_