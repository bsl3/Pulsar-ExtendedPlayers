#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <ExtendedPlayers/Core/CapacityContext.hpp>
#include <core/egg/mem/Heap.hpp>
extern "C" u32 EPScenarioOffset(u32);
namespace Pulsar { namespace ExtendedPlayers {
// Keep the 0x54-byte/four-screen Blooper ABI. Append racer scratch so ID 12 cannot overwrite +0x40 flags.
struct BlooperStorage { u8 native[0x54]; u32 groups[Capacity]; };
static_assert(sizeof(BlooperStorage) == 0xb4, "Blooper private storage");
static void* AllocateBlooper(u32) {
    void* storage = EGG::Heap::alloc(sizeof(BlooperStorage), 4);
    if(storage) memset(storage, 0, sizeof(BlooperStorage));
    return storage;
}
// Keep the native saved-LR store at +0x10; replace only operator new.
kmCall(0x807a8f20, AllocateBlooper);
extern "C" void EPClearBlooper(BlooperStorage* manager) {
    memset(manager->groups, 0, sizeof(manager->groups));
}
extern "C" u32 EPBlooperDelay(const u32* table, u32 byteOffset) {
    u32 index = byteOffset / 4;
    if(index >= VanillaCount) {
        const u32 count = Racedata::sInstance->racesScenario.playerCount;
        index = index * (VanillaCount - 1) / (count - 1);
        if(index >= VanillaCount) index = VanillaCount - 1;
    }
    return table[index];
}
extern "C" u32 EPBlooperRank(u32 oneBased) {
    u32 index = oneBased - 1;
    const u32 count = Racedata::sInstance->racesScenario.playerCount;
    if(IsActive()) index = index * (VanillaCount - 1) / (count - 1);
    return index;
}
asmFunc EPClearBlooper807a86b4() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r27;
        bl EPClearBlooper;
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a86b4, EPClearBlooper807a86b4);
kmWrite32(0x807a86b8, 0x901b0058); // Blooper clear entry: original 901b0014
kmWrite32(0x807a86bc, 0x901b005c); // Blooper clear entry: original 901b0018
kmWrite32(0x807a86c0, 0x901b0060); // Blooper clear entry: original 901b001c
kmWrite32(0x807a86c4, 0x901b0064); // Blooper clear entry: original 901b0020
kmWrite32(0x807a86c8, 0x901b0068); // Blooper clear entry: original 901b0024
kmWrite32(0x807a86cc, 0x901b006c); // Blooper clear entry: original 901b0028
kmWrite32(0x807a86d0, 0x901b0070); // Blooper clear entry: original 901b002c
kmWrite32(0x807a86d4, 0x901b0074); // Blooper clear entry: original 901b0030
kmWrite32(0x807a86d8, 0x901b0078); // Blooper clear entry: original 901b0034
kmWrite32(0x807a86dc, 0x901b007c); // Blooper clear entry: original 901b0038
kmWrite32(0x807a86e0, 0x901b0080); // Blooper clear entry: original 901b003c
asmFunc EPClearBlooper807a905c() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r27;
        bl EPClearBlooper;
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a905c, EPClearBlooper807a905c);
kmWrite32(0x807a9060, 0x901b0058); // Blooper clear entry: original 901b0014
kmWrite32(0x807a9064, 0x901b005c); // Blooper clear entry: original 901b0018
kmWrite32(0x807a9068, 0x901b0060); // Blooper clear entry: original 901b001c
kmWrite32(0x807a906c, 0x901b0064); // Blooper clear entry: original 901b0020
kmWrite32(0x807a9070, 0x901b0068); // Blooper clear entry: original 901b0024
kmWrite32(0x807a9074, 0x901b006c); // Blooper clear entry: original 901b0028
kmWrite32(0x807a9078, 0x901b0070); // Blooper clear entry: original 901b002c
kmWrite32(0x807a907c, 0x901b0074); // Blooper clear entry: original 901b0030
kmWrite32(0x807a9080, 0x901b0078); // Blooper clear entry: original 901b0034
kmWrite32(0x807a9084, 0x901b007c); // Blooper clear entry: original 901b0038
kmWrite32(0x807a9088, 0x901b0080); // Blooper clear entry: original 901b003c
asmFunc EPClearBlooper807a9744() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r3;
        bl EPClearBlooper;
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a9744, EPClearBlooper807a9744);
kmWrite32(0x807a9750, 0x90030058); // Blooper clear entry: original 90030014
kmWrite32(0x807a9754, 0x9003005c); // Blooper clear entry: original 90030018
kmWrite32(0x807a9758, 0x90030060); // Blooper clear entry: original 9003001c
kmWrite32(0x807a975c, 0x90030064); // Blooper clear entry: original 90030020
kmWrite32(0x807a9760, 0x90030068); // Blooper clear entry: original 90030024
kmWrite32(0x807a9764, 0x9003006c); // Blooper clear entry: original 90030028
kmWrite32(0x807a9768, 0x90030070); // Blooper clear entry: original 9003002c
kmWrite32(0x807a976c, 0x90030074); // Blooper clear entry: original 90030030
kmWrite32(0x807a9770, 0x90030078); // Blooper clear entry: original 90030034
kmWrite32(0x807a9774, 0x9003007c); // Blooper clear entry: original 90030038
kmWrite32(0x807a9778, 0x90030080); // Blooper clear entry: original 9003003c
kmWrite32(0x807a9330, 0x80180054); // Blooper racer group: original 80180010
kmWrite32(0x807a9448, 0x92780054); // Blooper racer group: original 92780010
kmWrite32(0x807a96b8, 0x92630054); // Blooper racer group: original 92630010
kmWrite32(0x807a9808, 0x90080054); // Blooper racer group: original 90080010
kmWrite32(0x807a9940, 0x90030054); // Blooper racer group: original 90030010
kmWrite32(0x807a999c, 0x801f0054); // Blooper racer group: original 801f0010
kmWrite32(0x807a8214, 0x3b800018); // Blooper no-player/iteration bound: original 3b80000c
kmWrite32(0x807a8aa8, 0x38a00018); // Blooper no-player/iteration bound: original 38a0000c
kmWrite32(0x807a83b4, 0x3bc00018); // Blooper no-player/iteration bound: original 3bc0000c
kmWrite32(0x807a843c, 0x38600018); // Blooper no-player/iteration bound: original 3860000c
kmWrite32(0x807a863c, 0x38800018); // Blooper no-player/iteration bound: original 3880000c
kmWrite32(0x807a8fe4, 0x38600018); // Blooper no-player/iteration bound: original 3860000c
kmWrite32(0x807a9180, 0x3ba00018); // Blooper no-player/iteration bound: original 3ba0000c
kmWrite32(0x807a94c8, 0x28170018); // Blooper no-player/iteration bound: original 2817000c
kmWrite32(0x807a9a14, 0x28000018); // Blooper no-player/iteration bound: original 2800000c
kmWrite32(0x807a9454, 0x2c1e0018); // Blooper no-player/iteration bound: original 2c1e000c
kmWrite32(0x807b1bf4, 0x38000018); // POW no-player sentinel: original 3800000c
kmWrite32(0x807b1cbc, 0x3bc00018); // POW no-player sentinel: original 3bc0000c
kmWrite32(0x807b1d28, 0x38000018); // POW no-player sentinel: original 3800000c
kmWrite32(0x807b248c, 0x3be00018); // POW no-player sentinel: original 3be0000c
kmWrite32(0x807b251c, 0x38600018); // POW no-player sentinel: original 3860000c
kmWrite32(0x807b2810, 0x3bc00018); // POW no-player sentinel: original 3bc0000c
kmWrite32(0x807b2868, 0x38800018); // POW no-player sentinel: original 3880000c
kmWrite32(0x807b2a0c, 0x2c040018); // POW no-player sentinel: original 2c04000c
kmWrite32(0x807b2be0, 0x38600018); // POW no-player sentinel: original 3860000c
asmFunc EPBlooperDelay807a9344() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r28;
        mr r4, r0;
        bl EPBlooperDelay;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a9344, EPBlooperDelay807a9344);
asmFunc EPBlooperDelay807a94e4() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r26;
        mr r4, r0;
        bl EPBlooperDelay;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a94e4, EPBlooperDelay807a94e4);
asmFunc EPBlooperDelay807a95c8() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r26;
        mr r4, r23;
        bl EPBlooperDelay;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a95c8, EPBlooperDelay807a95c8);
asmFunc EPBlooperRankIndex() {
    EP_SAVE_CONTEXT
    ASM(
        bl EPBlooperRank;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a992c, EPBlooperRankIndex);
asmFunc EPBlooperScenario807a97b8() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r4;
        bl EPScenarioOffset;
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a97b8, EPBlooperScenario807a97b8);
asmFunc EPBlooperScenario807a97d8() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r4;
        bl EPScenarioOffset;
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a97d8, EPBlooperScenario807a97d8);
asmFunc EPBlooperScenario807a9874() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r0;
        bl EPScenarioOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a9874, EPBlooperScenario807a9874);
asmFunc EPBlooperScenario807a9890() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r25;
        bl EPScenarioOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a9890, EPBlooperScenario807a9890);
asmFunc EPBlooperScenario807a99c0() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r0;
        bl EPScenarioOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807a99c0, EPBlooperScenario807a99c0);
} }
