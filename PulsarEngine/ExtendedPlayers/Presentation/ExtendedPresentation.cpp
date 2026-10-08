#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <ExtendedPlayers/Core/MemoryAccounting.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <core/egg/mem/Allocator.hpp>
#include <core/egg/mem/ExpHeap.hpp>

namespace Pulsar { namespace ExtendedPlayers {
// Section normally binds its layout allocator to structsHeaps.heaps[0].
// At 24 players, the result pages and race HUD share the available scene memory.
// Choose the heap when constructing the allocator; do not change it after layouts have been loaded.
static EGG::Allocator* CreateLayoutAllocator(EGG::Allocator* storage, EGG::Heap* nativeHeap, s32 alignment) {
    EGG::Heap* heap = nativeHeap;
    if(IsActive() && Racedata::sInstance->racesScenario.playerCount == Capacity) {
        const GameScene* scene = GameScene::GetCurrent();
        heap = scene->structsMem1->getAllocatableSize(alignment)
            > scene->structsMem2->getAllocatableSize(alignment) ? scene->structsMem1 : scene->structsMem2;
        OS::Report("[EP24] Section layout backing native=%p selected=%p largest=%u\n",
            nativeHeap, heap, heap->getAllocatableSize(alignment));
    }
    return new(storage) EGG::Allocator(heap, alignment);
}
kmCall(0x80621f1c, CreateLayoutAllocator);
kmCall(0x80621f54, CreateLayoutAllocator); // normal race sections take this native MEM2 branch

// Mii models need a 512 KiB temporary heap. Choose the scene heap with more room;
// MiiRenderMgr still owns it and frees it normally.
// The native call returns the child heap in r3 (the retail header declares void).
extern "C" EGG::ExpHeap* EPCreateMiiHeap(GameScene* scene, u32 size, u32 parent);
static EGG::ExpHeap* CreateMiiScratch(GameScene* scene, u32 size, u32 parent) {
    if(IsActive() && size == 0x80000 && parent == 1) {
        const u32 largest1 = scene->structsMem1->getAllocatableSize(-8);
        const u32 largest2 = scene->structsMem2->getAllocatableSize(-8);
        if(largest1 > largest2 && largest1 >= size) parent = 0;
        OS::Report("[EPMemory] Mii scratch bytes=%u parent=MEM%u MEM1 largest=%u MEM2 largest=%u\n",
            size, parent + 1, largest1, largest2);
    }
    EGG::ExpHeap* scratch = EPCreateMiiHeap(scene, size, parent);
    MemoryAccounting::Watch("MiiScratch", scratch);
    return scratch;
}
kmCall(0x80554570, CreateMiiScratch);

extern "C" void EPInitMiiRaceModels(void* manager);
static void InitMiiRaceModels(void* manager) {
    // Native MiiRenderMgr owns this temporary heap at +0x10. Its renderer
    // uses the same heap; ObjectsMgr frees it after copying the Mii heads.
    EGG::Heap* scratch = *reinterpret_cast<EGG::Heap**>(static_cast<u8*>(manager) + 0x10);
    if(IsActive()) OS::Report("[EPMemory] Mii models BEFORE scratch=%p largest=%u\n",
        scratch, scratch ? scratch->getAllocatableSize(32) : 0);
    EPInitMiiRaceModels(manager);
    MemoryAccounting::Report("Miis AFTER", true);
    if(IsActive()) OS::Report("[EPMemory] Mii models AFTER scratch=%p largest=%u\n",
        scratch, scratch ? scratch->getAllocatableSize(32) : 0);
}
kmCall(0x805545a0, InitMiiRaceModels);

// Log the audio heap without changing its size, parent or ownership.
static EGG::ExpHeap* CreateAudioHeap(int size, EGG::Heap* parent, u16 flags) {
    EGG::ExpHeap* child = EGG::ExpHeap::Create(size, parent, flags);
    MemoryAccounting::Watch("nativeAudio", child);
    MemoryAccounting::Report("audio reserved", false);
    return child;
}
kmCall(0x805542d8, CreateAudioHeap);
extern "C" void EPMemoryFinalizeScnMgr();
static void FinalizeScnMgr() {
    MemoryAccounting::Report("render finalize BEFORE", true);
    EPMemoryFinalizeScnMgr();
    MemoryAccounting::Report("render finalize AFTER", true);
}
kmCall(0x8051a850, FinalizeScnMgr);
} }
