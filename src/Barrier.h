#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace dm {

class Barrier
{
private:
    uint32_t        mCapacity = 1;

    uint32_t        mValue = 0;

    std::mutex              mLock;
    std::condition_variable mCV;

public:
    Barrier(uint32_t cap = 1)
    :
    mCapacity(cap)
    {

    }

    void post()
    {
        uint32_t v = 0;
        {
            std::lock_guard lock(mLock);
            v = ++mValue;
        }

        if (v == mCapacity)
        {
            mCV.notify_all();
        }
    }

    void waitAndReset()
    {
        std::unique_lock lock(mLock);
        mCV.wait(lock, [&] { return mValue == mCapacity; });
        mValue = 0;
    }

    void changeCapacity(uint32_t new_cap)
    {
        std::lock_guard lock(mLock);
        mCapacity = new_cap;
    }
};

}