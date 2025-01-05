#pragma once

#include "Barrier.h"
#include "Common.h"
#include "Connection.h"
#include "RequestGenerator.h"

#include "asio/io_context.hpp"
#include <atomic>
#include <cstdint>
#include <semaphore.h>

namespace dm {

// batch requests, and dispatch responses

// To reduce transfer and memcpy overhead, we can
    // 1. let Batcher gen the length of every request
    // 2. divide the buffer into continuous slices for each request
    // 3. give each request size and corresponding buf pos to all the clients
// and all the clients just need to put request data in the buffer.

// 1. batcher gen request lengths for clients
// 2. batcher wait clients finish generating
// 3. batcher send request batch to server
// 4. batcher recv response batch
// 5. batcher dispatch responses to clients

class Batcher
{
private:
    uint32_t                        mParallel = 16;
    std::vector<RequestGenerator *> mGenerators;
    std::vector<uint32_t>           mResponses;

    std::vector<std::thread>        mGenThreads;
    std::atomic<bool>               mStopped = false;
    Barrier                         mReadyBarrier;

    char           *mRequestBuffer = nullptr;

    ConnectionPtr   mConnection = nullptr;

    RNG             mRandom;

public:
    Batcher(uint32_t parallel)
    :
    mParallel(parallel),
    mResponses(mParallel, 0),
    mReadyBarrier(parallel),
    mRequestBuffer(new char[sizeof(Header) + mParallel * (1 + request_max_size)])
    {

    }

    ~Batcher()
    {
        for (auto gen : mGenerators) {
            delete gen;
        }

        delete[] mRequestBuffer;
    }

    bool init()
    {
        for (uint32_t i = 0; i < mParallel; i++)
        {
            auto gen = RequestGenerator::make(&mStopped, &mReadyBarrier);
            if (!gen) {
                return false;
            }

            mGenerators.emplace_back(gen);
            mGenThreads.emplace_back([gen]() { gen->run(); });
        }

        return true;
    }

    bool connect(asio::io_context &context, std::string host, std::string port)
    {
        mConnection = std::make_shared<Connection>(context);
        return mConnection->connect(host, port);
    }

    bool process(uint32_t count)
    {
        bool rslt = true;
        uint32_t original_count = count;
        while (count && rslt)
        {
            uint32_t size = count >= mParallel ? mParallel : count;
            uint32_t buf_size = dispatchRequests(size);
            rslt = buf_size != 0;
            if (!rslt)
                break;

            rslt = mConnection->send(mRequestBuffer, buf_size);
            if (!rslt)
                break;

#if defined(ENABLE_LOG)
            std::cout << "sent batch of " << size << " requests" << std::endl;
#endif

            rslt = processResponses(size);
            if (!rslt)
                break;

#if defined(ENABLE_LOG)
            std::cout << "recved all responses" << std::endl;
#endif

            count -= size;
        }

        std::cout << "finished " << original_count << " requests, stopping" << std::endl;

        mStopped.store(true, std::memory_order_release);
        for (uint32_t i = 0; i < mParallel; i++)
        {
            if (!mGenerators[i]->requestDistributed())
            {
                std::cerr << "error posting wait sem" << std::endl;
                return 0;
            }
        }

        return rslt;
    }

    void join()
    {
        for (uint32_t i = 0; i < mParallel; i++)
        {
            mGenThreads[i].join();
        }
    }

private:
    uint32_t dispatchRequests(Header size)
    {
        assert(size <= mParallel);
        if (size < mParallel)
        {
            mReadyBarrier.changeCapacity(size);
        }

        *((Header *)mRequestBuffer) = size;
        char *pos = mRequestBuffer + sizeof(Header);
        for (Header i = 0; i < size; i++)
        {
            int8_t len = mRandom.rand() % (request_max_size - request_min_size + 1) + request_min_size;
            mGenerators[i]->mRequestLength = len;
            mGenerators[i]->mRequestBuffer = pos;
            pos += len + 1;

            if (!mGenerators[i]->requestDistributed())
            {
                std::cerr << "error posting wait sem" << std::endl;
                return 0;
            }
        }

        mReadyBarrier.waitAndReset();

        uint32_t req_pos = sizeof(Header);
        for (Header i = 0; i < size; i++)
        {
            // if (!mGenerators[i]->waitForRequestReady())
            // {
            //     std::cerr << "error waiting for ready sem" << std::endl;
            //     return 0;
            // }

            uint32_t req_len = mGenerators[i]->mRequestLength;
            assert((uint32_t)mRequestBuffer[req_pos] == req_len);
            assert(req_len <= request_max_size);
            req_pos += req_len + 1;
        }

        return pos - mRequestBuffer;
    }

    bool processResponses(uint32_t size, bool verify = false)
    {
        assert(size <= mParallel);

        size_t nread = mConnection->recv((char *)mResponses.data(), size * sizeof(uint32_t));
        if (nread != size * sizeof(uint32_t))
        {
            return false;
        }

        if (!verify) {
            return true;
        }

        return true;
    }

};

}   // end of namespace dm
