#ifndef PUL_EXTENDED_MEMORY_HPP
#define PUL_EXTENDED_MEMORY_HPP
#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <core/egg/mem/ExpHeap.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar { namespace ExtendedPlayers {
// Choose a scene heap for the player Effect objects and arrays. Leave the particle/render allocators alone.
// Both scene heaps survive Effects::Mgr::Reset, and native delete finds the owning heap.
inline EGG::Heap* PlayerEffectBacking() {
    if(!IsActive()) return nullptr;
    const GameScene* scene = GameScene::GetCurrent();
    if(!scene || !scene->structsMem1 || !scene->structsMem2) return nullptr;
    return scene->structsMem2->getAllocatableSize(4) >= scene->structsMem1->getAllocatableSize(4)
        ? scene->structsMem2 : scene->structsMem1;
}
class PlayerEffectAllocation {
public:
    explicit PlayerEffectAllocation(u32 id) : previous(nullptr), player(id) {
        EGG::Heap* heap = PlayerEffectBacking();
        if(heap) {
            Report("effects BEGIN", heap);
            previous = heap->BecomeCurrentHeap();
        }
    }
    ~PlayerEffectAllocation() {
        if(previous) {
            EGG::Heap* selected = EGG::Heap::current;
            previous->BecomeCurrentHeap();
            Report("effects END", selected);
        }
    }
private:
    EGG::Heap* previous;
    u32 player;
    void Report(const char* phase, EGG::Heap* selected) {
        // One pair per racer during initialization, never per-frame.
        const GameScene* scene = GameScene::GetCurrent();
        OS::Report("[EPMemory] %s id=%u selected=%p MEM1 free=%u largest=%u MEM2 free=%u largest=%u\n",
            phase, player, selected, scene->structsMem1->getTotalFreeSize(),
            scene->structsMem1->getAllocatableSize(4), scene->structsMem2->getTotalFreeSize(),
            scene->structsMem2->getAllocatableSize(4));
    }
    PlayerEffectAllocation(const PlayerEffectAllocation&);
    PlayerEffectAllocation& operator=(const PlayerEffectAllocation&);
};
} }
#endif
