#pragma once

#include "Common.h"
#include "Connection.h"

#include "asio/io_context.hpp"
#include <cstdint>
#include <cstdlib>

namespace dm {

class Client
{
private:
    char            mRequestBuffer[1 + request_max_size];

    ConnectionPtr   mConnection = nullptr;

public:
    bool connect(asio::io_context &context, std::string host, std::string port)
    {
        mConnection = std::make_shared<Connection>(context);
        return mConnection->connect(host, port);
    }

    bool process(uint32_t count)
    {
        bool rslt = true;
        while (count-- && rslt)
        {
            rslt = processOnce();
        }
        return rslt;
    }

private:
    bool processOnce()
    {
        uint32_t req_len = generateRequest();

        // int i = 1;
        // ::setsockopt(mConnection->getSocket().native_handle(), IPPROTO_TCP, TCP_QUICKACK, &i, sizeof(i));
        bool rslt = mConnection->send(mRequestBuffer, req_len + 1);
        if (!rslt) {
            return false;
        }

        // std::cout << "client sent req '" << std::string_view(mRequestBuffer + 1, req_len) << "', len: " << req_len << std::endl;

        uint32_t crc32;
        // ::setsockopt(mConnection->getSocket().native_handle(), IPPROTO_TCP, TCP_QUICKACK, &i, sizeof(i));
        size_t nread = mConnection->recv((char *)&crc32, sizeof(crc32));
        if (nread != sizeof(crc32)) {
            return false;
        }

        // std::cout << "client recv crc32: " << crc32 << std::endl;

        return true;
    }

    uint32_t generateRequest()
    {
        // rand() might be not thread-safe, but it should be okay
        int8_t len = rand() % (request_max_size - request_min_size + 1) + request_min_size;
        *mRequestBuffer = len;

        char *pos = mRequestBuffer + 1;
        for (int l = len; l > 0; l -= 5)
        {
            generateSequence(pos, l >= 5 ? 5 : l);
        }

        return len;
    }

    void generateSequence(char *&pos, int size)
    {
        // 0-9a-zA-Z has 62 characters.
        // 62^5 < 64^5 = 2^30, which is in the range of int32_t,
        // so we can use 1 rand() call to generate up to 5 characters at once.
        assert(size <= 5);

        int32_t result = rand();
        for (; size > 0; --size)
        {
            int bits = result & 0x0000003F;
            if (bits < 10)
            {
                *pos = '0' + bits;
            }
            else if (bits < 36)
            {
                *pos = 'A' + bits - 10;
            }
            else
            {
                *pos = 'a' + bits - 36;
            }

            result = result >> 6;
            ++pos;
        }
    }
};

}   // end of namespace dm
