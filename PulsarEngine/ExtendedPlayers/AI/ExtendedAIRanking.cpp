#include <ExtendedPlayers/ExtendedPlayers.hpp>

extern "C" {
void EPOriginalSortAICPU(void*);
void EPOriginalSortAIHuman(void*);
}

namespace Pulsar { namespace ExtendedPlayers {
// Sub84 has interleaved source/sorted pointer pairs: CPU at E8/EC (12),
// human at 148/14C (4), with counts at 178/17C. Native sorting buckets
// by race rank into twelve stack slots, so extended ranks lose entries.
static void SortGroup(void* owner, u32 pairOffset, u32 countOffset, bool cpu) {
    u8* base = static_cast<u8*>(owner);
    const u32 count = *reinterpret_cast<u32*>(base + countOffset);
    // Sort existing participants directly instead of indexing scratch by rank.
    // No allocation, registration changes, or writes to Raceinfo positions.
    void* sorted[Capacity];
    for(u32 i = 0; i < count; ++i) {
        void* entry = cpu ? CPUGroupEntry(owner, i, false)
            : *reinterpret_cast<void**>(base + pairOffset + i * 8);
        const s32 rank = *reinterpret_cast<s32*>(static_cast<u8*>(entry) + 0x14);
        u32 j = i;
        while(j && *reinterpret_cast<s32*>(static_cast<u8*>(sorted[j - 1]) + 0x14) > rank) {
            sorted[j] = sorted[j - 1]; --j;
        }
        sorted[j] = entry;
    }
    for(u32 i = 0; i < count; ++i) {
        if(cpu) CPUGroupEntry(owner, i, true) = sorted[i];
        else *reinterpret_cast<void**>(base + pairOffset + 4 + i * 8) = sorted[i];
        // Native CPU sorter assigns group-relative ranks; human keeps race rank.
        if(cpu) *reinterpret_cast<s32*>(static_cast<u8*>(sorted[i]) + 0x14) = i + 1;
    }
}
static void SortCPU(void* owner) {
    if(!IsActive()) { EPOriginalSortAICPU(owner); return; }
    SortGroup(owner, 0xe8, 0x178, true);
}
static void SortHuman(void* owner) {
    if(!IsActive()) { EPOriginalSortAIHuman(owner); return; }
    SortGroup(owner, 0x148, 0x17c, false);
}
kmCall(0x80741700, SortCPU);
kmCall(0x80741708, SortHuman);
} }
