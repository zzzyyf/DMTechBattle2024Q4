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

    std::vector<uint32_t>   mResponses;

    std::atomic<bool>   mStopped = false;

public:
    Server(asio::io_context &context, const std::string &host, const std::string &port)
    :
    mContext(context),
    mHost(host),
    mPort(port),
    mAcceptor(context)
    {
        mResponses.resize(20000);
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

        // connection->getSocket().set_option(tcp::no_delay(true));

        // mConnections.emplace_back(connection);
        mThreads.emplace_back([&, connection](){
            char buf[1 + request_max_size];
            while (!mStopped.load(std::memory_order_acquire))
            {
                bool rslt = processV2(connection, buf, 1 + request_max_size);
                // bool rslt = process(connection, buf, 1 + request_max_size);
                if (!rslt) {
                    break;
                }
            }
        });

        ConnectionPtr new_conn = std::make_shared<Connection>(mContext);
        mAcceptor.async_accept(new_conn->getSocket(), std::bind(&Server::handleAccept, this, new_conn, asio::placeholders::error));
    }

    static bool process(ConnectionPtr connection, char *recvbuf, const uint32_t len)
    {
        // int i = 1;
        // ::setsockopt(connection->getSocket().native_handle(), IPPROTO_TCP, TCP_QUICKACK, &i, sizeof(i));

        int8_t req_len = connection->recvReq(recvbuf, len);
        if ((uint32_t)req_len < request_min_size || (uint32_t)req_len > request_max_size)
        {
            return false;
        }

        // std::cout << "server received req '" << std::string_view(recvbuf, req_len) << "', len: " << req_len << std::endl;

        // ::setsockopt(connection->getSocket().native_handle(), IPPROTO_TCP, TCP_QUICKACK, &i, sizeof(i));

        uint32_t crc = ::crc32((const unsigned char *)recvbuf, req_len);
        connection->send((const char*)&crc, sizeof(crc));

        // std::cout << "server sent crc32: " << crc << std::endl;

        return true;
    }

    bool processV2(ConnectionPtr connection, char *recvbuf, const uint32_t len)
    {
        // int i = 1;
        // ::setsockopt(connection->getSocket().native_handle(), IPPROTO_TCP, TCP_QUICKACK, &i, sizeof(i));
        Header batch_size;
        auto nread = connection->recv((char *)&batch_size, sizeof(Header));
        assert(nread == sizeof(Header));

        uint32_t *resp_pos = mResponses.data();
        for (Header i = 0; i < batch_size; i++)
        {
            int8_t req_len = connection->recvReq(recvbuf, len);
            if ((uint32_t)req_len < request_min_size || (uint32_t)req_len > request_max_size)
            {
                return false;
            }

#if defined(ENABLE_LOG)
            std::cout << "server received req " << (int)i << ": '" << std::string_view(recvbuf, req_len) << "', len: " << (int)req_len << std::endl;
#endif

            // ::setsockopt(connection->getSocket().native_handle(), IPPROTO_TCP, TCP_QUICKACK, &i, sizeof(i));

            *resp_pos = ::crc32((const unsigned char *)recvbuf, req_len);
            ++resp_pos;
        }
        connection->send((const char*)mResponses.data(), batch_size * sizeof(uint32_t));

#if defined(ENABLE_LOG)
        std::cout << "server sent responses: " << (int)batch_size << std::endl;
#endif

        return true;
    }
};

}