/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "svga3_fifo_framing.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <random>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while (0)
int main() {
    using namespace svga3_vlkn;
    std::mt19937 random(0x5a17);
    std::array<uint32_t,64> packet{};
    const uint32_t commands[]{SVGA_CMD_ESCAPE, SVGA_CMD_DEFINE_SCREEN,
        SVGA_CMD_DEFINE_CURSOR, SVGA_CMD_DEFINE_ALPHA_CURSOR,
        SVGA_CMD_REMAP_GMR2, SVGA_CMD_FENCE, SVGA_3D_CMD_BASE};
    for (unsigned trial=0;trial<20000;++trial) {
        for (auto &word:packet) word=random();
        if (trial%2) packet[0]=commands[trial%7];
        const unsigned available=random()%(packet.size()+1);
        bool outOfBounds=false;
        auto read=[&](uint64_t index) {
            if (index>=available) { outOfBounds=true; return uint32_t(0); }
            return packet[index];
        };
        uint64_t words=0;
        const auto status=fifoFrame(available,read,words);
        CHECK(!outOfBounds);
        if (status==FifoFrameStatus::Complete) CHECK(words>0 && words<=available);
    }
    // Every truncation of each bounded variable-length payload remains incomplete.
    for (uint32_t bytes=0;bytes<128;++bytes) {
        for (bool escape:{false,true}) {
            packet.fill(0);
            packet[0]=escape ? SVGA_CMD_ESCAPE : SVGA_3D_CMD_BASE;
            packet[escape ? 2 : 1]=bytes;
            const uint64_t expected=(escape ? 3 : 2)+(bytes+3)/4;
            for (uint64_t available=0;available<=expected;++available) {
                bool outOfBounds=false;
                auto read=[&](uint64_t i) { if (i>=available) { outOfBounds=true; return uint32_t(0); } return packet[i]; };
                uint64_t words=0;
                const auto status=fifoFrame(available,read,words);
                CHECK(!outOfBounds);
                CHECK(status==(available==expected ? FifoFrameStatus::Complete : FifoFrameStatus::Incomplete));
                if (available==expected) CHECK(words==expected);
            }
        }
    }
    std::puts("FIFO seeded malformed headers and exhaustive payload truncations: PASS");
}
