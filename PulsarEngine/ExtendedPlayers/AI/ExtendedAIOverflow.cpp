#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <MarioKartWii/AI/AIManager.hpp>

namespace Pulsar { namespace ExtendedPlayers {
// Sub84's twelve CPU pairs end exactly where its four human pairs begin.
// Keep Nintendo's layout intact. Only overflow entries live outside the owner.
static void* overflow[Capacity - VanillaCount][2];
void ResetAIOverflow() { memset(overflow, 0, sizeof(overflow)); }
void*& CPUGroupEntry(void* owner, u32 index, bool sorted) {
    if(IsActive() && index >= VanillaCount && index < Capacity)
        return overflow[index - VanillaCount][sorted ? 1 : 0];
    return *reinterpret_cast<void**>(reinterpret_cast<u8*>(owner) + 0xe8 + index * 8 + (sorted ? 4 : 0));
}
extern "C" void** EPAICPUAddress(void* base, u32 field) {
    if(IsActive()) {
        void* owner = AI::Manager::sInstance->specialItemStruct;
        const u32 delta = reinterpret_cast<u32>(base) - reinterpret_cast<u32>(owner);
        if(owner && !(delta & 7) && delta < Capacity * 8)
            return &CPUGroupEntry(owner, delta / 8, field == 0xec);
    }
    return reinterpret_cast<void**>(static_cast<u8*>(base) + field);
}
extern "C" u32 EPAILastGroupCount(u32 authored) {
    if(!IsActive()) return authored;
    const u32 count = *reinterpret_cast<u32*>(
        reinterpret_cast<u8*>(AI::Manager::sInstance->specialItemStruct) + 0x178);
    // The native 12-CPU split is 3/6/3. Add the next three CPUs to the last group;
    // each group has six slots.
    return authored + (count > VanillaCount ? count - VanillaCount : 0);
}
} }
