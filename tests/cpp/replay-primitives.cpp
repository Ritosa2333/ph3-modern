#include "MersenneTwister.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>

static void bits(double value) {
    std::uint64_t raw;std::memcpy(&raw,&value,8);
    std::printf("\"%016llx\"",(unsigned long long)raw);
}
int main() {
    std::printf("{\"longDoubleBytes\":%u,\"longDoubleDigits\":%d,\"cases\":[",unsigned(sizeof(long double)),std::numeric_limits<long double>::digits);
    const unsigned seeds[]={0,1,396666165,0xffffffffu};
    for(unsigned s=0;s<4;++s){
        if(s)std::printf(",");gstd::MersenneTwister random(seeds[s]);
        std::printf("{\"seed\":%u,\"raw\":[",seeds[s]);
        std::mt19937 raw(seeds[s]);
        for(int i=0;i<8;++i){if(i)std::printf(",");std::printf("%u",unsigned(raw()));}
        std::printf("],\"int\":[");
        for(int i=0;i<16;++i){if(i)std::printf(",");std::printf("%ld",random.GetInt(-123,456));}
        std::printf("],\"real\":[");
        for(int i=0;i<16;++i){if(i)std::printf(",");bits(double(random.GetReal(-180,360)));}
        std::printf("],\"int64\":[");
        for(int i=0;i<8;++i){if(i)std::printf(",");std::printf("\"%lld\"",(long long)random.GetInt64(-INT64_C(1000000000000),INT64_C(2000000000000)));}
        std::printf("]}");
    }
    const volatile long double big=9007199254740992.0L;
    std::printf("],\"numeric\":{");
    std::printf("\"largeAddSubtract\":");bits(double((big+1.0L)-big));
    std::printf("}}\n");return 0;
}
