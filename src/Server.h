#pragma once

#include "Common.h"
#include "Connection.h"

#include "asio/io_context.hpp"
#include "asio/ip/address.hpp"
#include "asio/placeholders.hpp"
#include "iberty/crc32.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>

namespace dm {

class Server
{
private:
    asio::io_context    &mContext;

    std::string         mHost;
    std::string         mPort;

    tcp::acceptor       mAcceptor;

    // std::vector<ConnectionPtr>  mConnections;
    std::vector<std::thread>    mThreads;

    std::atomic<bool>   mStopped = false;

public:
    Server(asio::io_context &context, const std::string &host, const std::string &port)
    :
    mContext(context),
    mHost(host),
    mPort(port),
    mAcceptor(context)
    {

    }

    ~Server()
    {
        mAcceptor.close();
        stop();
    }

    bool start()
    {
        tcp::resolver resolver(mContext);
        try {
            auto resolve_result = resolver.resolve(mHost, mPort);
            tcp::endpoint endpoint = *resolve_result.begin();
            std::cout << "server listen endpoint: " << endpoint << std::endl;

            mAcceptor.open(endpoint.protocol());
            mAcceptor.set_option(tcp::acceptor::reuse_address(true));
            mAcceptor.bind(endpoint);
            mAcceptor.listen();
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << std::endl;
            return false;
        }

        ConnectionPtr connection = std::make_shared<Connection>(mContext);
        mAcceptor.async_accept(connection->getSocket(), std::bind(&Server::handleAccept, this, connection, asio::placeholders::error));

        return true;
    }

    void stop()
    {
        mStopped.store(false, std::memory_order_release);
        for (auto &thread : mThreads)
        {
            thread.join();
        }
    }

    void handleAccept(ConnectionPtr connection, const std::error_code &error)
    {
        if (error)
        {
            std::cerr << error.message() << std::endl;
            return;
        }

        // mConnections.emplace_back(connection);
        mThreads.emplace_back([&](){
            char buf[1 + request_max_size];
            while (!mStopped.load(std::memory_order_acquire))
            {
                bool rslt = process(connection, buf, 1 + request_max_size);
                if (!rslt) {
                    break;
                }
            }
        });

        ConnectionPtr new_conn = std::make_shared<Connection>(mContext);
        mAcceptor.async_accept(new_conn->getSocket(), std::bind(&Server::handleAccept, this, new_conn, asio::placeholders::error));
    }

    static bool process(ConnectionPtr &connection, char *recvbuf, const uint32_t len)
    {
        int8_t req_len = connection->recvReq(recvbuf, len);
        if ((uint32_t)req_len < request_min_size || (uint32_t)req_len > request_max_size)
        {
            return false;
        }

        uint32_t crc = ::crc32((const unsigned char *)recvbuf, req_len);
        connection->send((const char*)&crc, sizeof(crc));

        return true;
    }
};

}