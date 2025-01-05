#pragma once
#include "Barrier.h"
#include "Common.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include "semaphore.h"


namespace dm {

class RequestGenerator
{
public:
    std::atomic<bool>   *mStopped = nullptr;

    char       *mRequestBuffer = nullptr;
    uint32_t    mRequestLength = 0;

private:
    // sem_t   *mWaitSem = nullptr;
    // sem_t   *mReadySem = nullptr;

    Barrier     mDistBarrier;
    Barrier    *mReadyBarrier = nullptr;

    std::string mAlphabets;
    RNG         mRandom;

private:
    // RequestGenerator(std::atomic<bool> *stopped)
    RequestGenerator(std::atomic<bool> *stopped, Barrier *ready_barrier)
    :
    mStopped(stopped),
    mReadyBarrier(ready_barrier)
    {
        assert(mReadyBarrier);
    }

public:
    ~RequestGenerator()
    {
        // if (mWaitSem)
        //     sem_destroy(mWaitSem);

        // if (mReadySem)
        //     sem_destroy(mReadySem);
    }

private:
    bool init()
    {
        mAlphabets.resize(62 + 2);
        int pos = 0;
        for (char c = '0'; c <= '9'; c++) {
            mAlphabets[pos++] = c;
        }
        for (char c = 'A'; c <= 'Z'; c++) {
            mAlphabets[pos++] = c;
        }
        for (char c = 'a'; c <= 'z'; c++) {
            mAlphabets[pos++] = c;
        }
        // paddings
        for (int i = 0; i < 2; i++) {
            mAlphabets[pos++] = '0' + i;
        }

        // mWaitSem = new sem_t();
        // auto ret = sem_init(mWaitSem, 0/*pshared*/, 0/*value*/);
        // if (ret != 0)
        // {
        //     std::cerr << "error initializing wait sem" << std::endl;
        //     return false;
        // }

        // mReadySem = new sem_t();
        // ret = sem_init(mReadySem, 0/*pshared*/, 0/*value*/);
        // if (ret != 0)
        // {
        //     std::cerr << "error initializing wait sem" << std::endl;
        //     return false;
        // }

        return true;
    }

public:
    void run()
    {
        while (true)
        {
            if (!waitForRequestDist())
            {
                std::cerr << "error waiting for wait sem" << std::endl;
                return;
            }
            if (mStopped->load(std::memory_order_acquire)) {
                return;
            }

            generateRequest();

            if (!requestReady())
            {
                std::cerr << "error posting ready sem" << std::endl;
                return;
            }
        }
    }

    template <class... Args>
    static RequestGenerator *make(Args &&... args)
    {
        auto gen = new RequestGenerator(std::forward<Args>(args)...);
        if (gen->init()) {
            return gen;
        }
        return nullptr;
    }

    inline bool requestDistributed()
    {
        // return sem_post(mWaitSem) == 0;
        mDistBarrier.post();
        return true;
    }

    inline bool waitForRequestDist()
    {
        // return sem_wait(mWaitSem) == 0;
        mDistBarrier.waitAndReset();
        return true;
    }

    inline bool requestReady()
    {
        // return sem_post(mReadySem) == 0;
        mReadyBarrier->post();
        return true;
    }

    inline bool waitForRequestReady()
    {
        // return sem_wait(mReadySem) == 0;
        mReadyBarrier->waitAndReset();
        return true;
    }

private:
    void generateRequest()
    {
        *mRequestBuffer = mRequestLength;

        char *pos = mRequestBuffer + 1;
        for (int l = mRequestLength; l > 0; l -= 5)
        {
            generateSequence(pos, l >= 5 ? 5 : l);
        }
    }

    void generateSequence(char *&pos, int size)
    {
        // 0-9a-zA-Z has 62 characters.
        // 62^5 < 64^5 = 2^30, which is in the range of int32_t,
        // so we can use 1 rand() call to generate up to 5 characters at once.
        assert(size <= 10);

        uint64_t result = mRandom.rand();
        for (; size > 0; --size)
        {
            uint64_t bits = result & 0x0000003F;
            *pos = mAlphabets[bits];

            result = result >> 6;
            ++pos;
        }
    }

};

}