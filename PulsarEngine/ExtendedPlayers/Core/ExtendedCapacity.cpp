#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <MarioKartWii/AI/AIManager.hpp>
#include <core/egg/mem/Heap.hpp>

extern "C" void EPPacingShuffleResume();
// Replay the prologue; native-size groups keep the original permutation algorithm and RNG.
asmFunc EPPacingShuffleOriginal() {
    ASM(nofralloc; stwu r1, -0x20(r1); b EPPacingShuffleResume;)
}
namespace Pulsar { namespace ExtendedPlayers {
struct PacingGroupStorage {
    u8 native[0x30];
    void* members[Capacity];
};
static_assert(sizeof(PacingGroupStorage) == 0x90, "Pacing storage/operand offsets");
static void* AllocatePacingGroup(u32) {
    void* group = EGG::Heap::alloc(sizeof(PacingGroupStorage), 4);
    if(group) memset(group, 0, sizeof(PacingGroupStorage));
    return group;
}
// Hook allocation after native counters are cleared. +0x120 initializes state; it is not an allocation.
kmCall(0x80741138, AllocatePacingGroup);
kmCall(0x80741150, AllocatePacingGroup);
kmCall(0x80741168, AllocatePacingGroup);
static void ShufflePacingGroup(PacingGroupStorage* group) {
    const u32 count = *reinterpret_cast<u32*>(group->native + 4);
    if(count <= 6) {
        reinterpret_cast<void (*)(void*)>(EPPacingShuffleOriginal)(group);
        return;
    }
    // Groups above twelve overflow u32 factorials and native scratch. Use Fisher-Yates with the same RNG.
    u8 ranks[Capacity];
    for(u32 i = 0; i < count; ++i) ranks[i] = i;
    for(u32 n = count; n > 1; --n) {
        const u32 j = AI::Manager::sInstance->GetRandomValue(n);
        const u8 temp = ranks[n - 1]; ranks[n - 1] = ranks[j]; ranks[j] = temp;
    }
    const u32 start = *reinterpret_cast<u32*>(group->native + 8);
    for(u32 i = 0; i < count; ++i)
        *reinterpret_cast<u32*>(static_cast<u8*>(group->members[i]) + 0x18) = start + ranks[i];
}
kmBranch(0x8073f848, ShufflePacingGroup);
// Redirect only the verified member accesses; state fields +0x24/+0x28/+0x2c stay in place.
kmWrite32(0x8073f688, 0x90030030); // pacing member: original 9003000c
kmWrite32(0x8073f68c, 0x90030034); // pacing member: original 90030010
kmWrite32(0x8073f690, 0x90030038); // pacing member: original 90030014
kmWrite32(0x8073f694, 0x9003003c); // pacing member: original 90030018
kmWrite32(0x8073f698, 0x90030040); // pacing member: original 9003001c
kmWrite32(0x8073f69c, 0x90030044); // pacing member: original 90030020
kmWrite32(0x8073f6e8, 0x90030030); // pacing member: original 9003000c
kmWrite32(0x8073f6ec, 0x90030034); // pacing member: original 90030010
kmWrite32(0x8073f6f0, 0x90030038); // pacing member: original 90030014
kmWrite32(0x8073f6f4, 0x9003003c); // pacing member: original 90030018
kmWrite32(0x8073f6f8, 0x90030040); // pacing member: original 9003001c
kmWrite32(0x8073f6fc, 0x90030044); // pacing member: original 90030020
kmWrite32(0x8073f718, 0x90030030); // pacing member: original 9003000c
kmWrite32(0x8073f71c, 0x90030034); // pacing member: original 90030010
kmWrite32(0x8073f720, 0x90030038); // pacing member: original 90030014
kmWrite32(0x8073f724, 0x9003003c); // pacing member: original 90030018
kmWrite32(0x8073f728, 0x90030040); // pacing member: original 9003001c
kmWrite32(0x8073f72c, 0x90030044); // pacing member: original 90030020
kmWrite32(0x8073f744, 0x90850030); // pacing member: original 9085000c
kmWrite32(0x8073f754, 0x80840030); // pacing member: original 8084000c
kmWrite32(0x8073f780, 0x80c30030); // pacing member: original 80c3000c
kmWrite32(0x8073f838, 0x80630030); // pacing member: original 8063000c
kmWrite32(0x8073f8b8, 0x80660030); // pacing member: original 8066000c
kmWrite32(0x8073f8cc, 0x9003003c); // pacing member: original 90030018
kmWrite32(0x8073f908, 0x80850030); // pacing member: original 8085000c
kmWrite32(0x8073f980, 0x83a30030); // pacing member: original 83a3000c
kmWrite32(0x8073f9dc, 0x80630030); // pacing member: original 8063000c
kmWrite32(0x8073fa2c, 0x80e50030); // pacing member: original 80e5000c
kmWrite32(0x8073fa68, 0x80a60030); // pacing member: original 80a6000c
kmWrite32(0x8073fa94, 0x80850030); // pacing member: original 8085000c
kmWrite32(0x8073fb0c, 0x837d0030); // pacing member: original 837d000c
kmWrite32(0x8073fb94, 0x90030030); // pacing member: original 9003000c
kmWrite32(0x8073fb98, 0x90030034); // pacing member: original 90030010
kmWrite32(0x8073fb9c, 0x90030038); // pacing member: original 90030014
kmWrite32(0x8073fba0, 0x9003003c); // pacing member: original 90030018
kmWrite32(0x8073fba4, 0x90030040); // pacing member: original 9003001c
kmWrite32(0x8073fba8, 0x90030044); // pacing member: original 90030020
kmWrite32(0x8073fbfc, 0x90030030); // pacing member: original 9003000c
kmWrite32(0x8073fc00, 0x90030034); // pacing member: original 90030010
kmWrite32(0x8073fc04, 0x90030038); // pacing member: original 90030014
kmWrite32(0x8073fc08, 0x9003003c); // pacing member: original 90030018
kmWrite32(0x8073fc0c, 0x90030040); // pacing member: original 9003001c
kmWrite32(0x8073fc10, 0x90030044); // pacing member: original 90030020
kmWrite32(0x8073fc70, 0x807d0030); // pacing member: original 807d000c
kmWrite32(0x8073fd0c, 0x807e0030); // pacing member: original 807e000c
kmWrite32(0x8073fe10, 0x83830030); // pacing member: original 8383000c
kmWrite32(0x8073fe6c, 0x80630030); // pacing member: original 8063000c
kmWrite32(0x8073feac, 0x80850030); // pacing member: original 8085000c
kmWrite32(0x8073ff90, 0x83a30030); // pacing member: original 83a3000c
kmWrite32(0x8073ffec, 0x80630030); // pacing member: original 8063000c
kmWrite32(0x80740008, 0x839f0030); // pacing member: original 839f000c
kmWrite32(0x80740078, 0x80630030); // pacing member: original 8063000c
kmWrite32(0x80740120, 0x83830030); // pacing member: original 8383000c
kmWrite32(0x8074017c, 0x80630030); // pacing member: original 8063000c
kmWrite32(0x8074023c, 0x83830030); // pacing member: original 8383000c
kmWrite32(0x80740298, 0x80630030); // pacing member: original 8063000c
kmWrite32(0x807402d8, 0x80850030); // pacing member: original 8085000c
kmWrite32(0x807403c8, 0x83a30030); // pacing member: original 83a3000c
kmWrite32(0x80740424, 0x80630030); // pacing member: original 8063000c
kmWrite32(0x80740440, 0x80bf0030); // pacing member: original 80bf000c
kmWrite32(0x80740464, 0x80660030); // pacing member: original 8066000c
kmWrite32(0x80740568, 0x83a30030); // pacing member: original 83a3000c
kmWrite32(0x807405c4, 0x80630030); // pacing member: original 8063000c
kmWrite32(0x80740648, 0x80870030); // pacing member: original 8087000c
kmWrite32(0x80740694, 0x83bb0030); // pacing member: original 83bb000c
kmWrite32(0x80740718, 0x80860030); // pacing member: original 8086000c
kmWrite32(0x80740754, 0x90030030); // pacing member: original 9003000c
kmWrite32(0x80740758, 0x90030034); // pacing member: original 90030010
kmWrite32(0x8074075c, 0x90030038); // pacing member: original 90030014
kmWrite32(0x80740760, 0x9003003c); // pacing member: original 90030018
kmWrite32(0x80740764, 0x90030040); // pacing member: original 9003001c
kmWrite32(0x80740768, 0x90030044); // pacing member: original 90030020
kmWrite32(0x807407b8, 0x90030030); // pacing member: original 9003000c
kmWrite32(0x807407bc, 0x90030034); // pacing member: original 90030010
kmWrite32(0x807407c0, 0x90030038); // pacing member: original 90030014
kmWrite32(0x807407c4, 0x9003003c); // pacing member: original 90030018
kmWrite32(0x807407c8, 0x90030040); // pacing member: original 9003001c
kmWrite32(0x807407cc, 0x90030044); // pacing member: original 90030020
kmWrite32(0x80740888, 0x83830030); // pacing member: original 8383000c
kmWrite32(0x807408e4, 0x80630030); // pacing member: original 8063000c
kmWrite32(0x80740944, 0x80bf0030); // pacing member: original 80bf000c
kmWrite32(0x807409f8, 0x80870030); // pacing member: original 8087000c
kmWrite32(0x80740a44, 0x83bb0030); // pacing member: original 83bb000c
kmWrite32(0x80740ac8, 0x80860030); // pacing member: original 8086000c
kmWrite32(0x80740b04, 0x90030030); // pacing member: original 9003000c
kmWrite32(0x80740b08, 0x90030034); // pacing member: original 90030010
kmWrite32(0x80740b0c, 0x90030038); // pacing member: original 90030014
kmWrite32(0x80740b10, 0x9003003c); // pacing member: original 90030018
kmWrite32(0x80740b14, 0x90030040); // pacing member: original 9003001c
kmWrite32(0x80740b18, 0x90030044); // pacing member: original 90030020
kmWrite32(0x80740b68, 0x90030030); // pacing member: original 9003000c
kmWrite32(0x80740b6c, 0x90030034); // pacing member: original 90030010
kmWrite32(0x80740b70, 0x90030038); // pacing member: original 90030014
kmWrite32(0x80740b74, 0x9003003c); // pacing member: original 90030018
kmWrite32(0x80740b78, 0x90030040); // pacing member: original 9003001c
kmWrite32(0x80740b7c, 0x90030044); // pacing member: original 90030020
kmWrite32(0x80740bd8, 0x80870030); // pacing member: original 8087000c
kmWrite32(0x80740c24, 0x83bb0030); // pacing member: original 83bb000c
kmWrite32(0x80740ca8, 0x80860030); // pacing member: original 8086000c
kmWrite32(0x80511518, 0x5498073e); // checkpoint search token: original 7c982378
} }
