#pragma once

#include "Common.h"
#include "asio/asio.hpp"
#include "asio/io_context.hpp"
#include <iostream>
#include <memory>
#include <string>

namespace dm {

using asio::ip::tcp;

class Connection
{
private:
    asio::io_context    &mContext;
    tcp::socket         mSocket;

public:
    Connection(asio::io_context &context)
    :
    mContext(context),
    mSocket(mContext)
    {

    }

    ~Connection()
    {
        if (mSocket.is_open())
        {
            mSocket.close();
        }
    }

    tcp::socket &getSocket() { return mSocket; }

    // connect
    bool connect(const std::string &host, const std::string &port)
    {
        tcp::resolver resolver(mContext);
        try {
            auto resolve_result = resolver.resolve(host, port);
            asio::connect(mSocket, resolve_result);
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << std::endl;
            return false;
        }

        return true;
    }

    // send
    bool send(const char *buf, int32_t len)
    {
        std::error_code error;
        size_t nsend = asio::write(mSocket, asio::buffer(buf, len), error);
        if (error)
        {
            std::cerr << error.message() << std::endl;
            return false;
        }
        assert(nsend == len);
        return true;
    }

    // recv
    size_t recv(char *buf, int32_t capacity)
    {
        std::error_code error;
        size_t nread = asio::read(mSocket, asio::buffer(buf, capacity), error);
        if (error)
        {
            std::cerr << error.message() << std::endl;
            return 0;
        }
        return nread;
    }

    size_t recvReq(char *buf, int32_t capacity)
    {
        std::error_code error;

        // recv head
        int8_t req_len = 0;
        size_t nread = mSocket.read_some(asio::buffer((void *)&req_len, 1), error);
        if (error)
        {
            std::cerr << error.message() << std::endl;
            return 0;
        }
        assert((uint32_t)req_len >= request_min_size && (uint32_t)req_len <= request_max_size && req_len <= capacity);

        // recv body
        nread = asio::read(mSocket, asio::buffer(buf, req_len), error);
        if (error)
        {
            std::cerr << error.message() << std::endl;
            return 0;
        }
        return nread;
    }
};

using ConnectionPtr = Ptr<Connection>;

}   // end of namespace dm
