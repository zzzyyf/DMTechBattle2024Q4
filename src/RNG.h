#pragma once
#include <cassert>
#include <cstdint>
#include <sys/random.h>

namespace dm {

class RNG
{
private:
    __uint128_t mLehmerState;

public:
    RNG()
    {
        char *buf = (char *)&mLehmerState;
        int rslt = getrandom(buf, 16/*length*/, 0/*flags*/);
        assert(rslt == 0);
    }

    uint64_t rand()
    {
        mLehmerState *= 0xda942042e4dd58b5;
        return mLehmerState >> 64;
    }
};

}   // end of namespace dm
