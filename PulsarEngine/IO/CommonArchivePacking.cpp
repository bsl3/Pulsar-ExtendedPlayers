#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>

// Only declare IsActive here; including the full header would add layout initializers.
namespace Pulsar { namespace ExtendedPlayers { bool IsActive(); } }

namespace Pulsar {
// Mount Common before allocating the kart archive heap. Otherwise those later mounts
// leave a free block inside the parent that adjust() cannot return to the race.
// Use the native loader and its task wait; the later LoadArchive/AddArchivesHolder calls still run.
static EGG::ExpHeap* CreatePackedKartArchiveHeap(int size, EGG::Heap* parent, u16 flags) {
    const GameScene* scene=GameScene::GetCurrent();
    if(ExtendedPlayers::IsActive() && scene && scene->id==SCENE_ID_RACE
        && parent && parent==scene->archiveHeapMem1 && ArchiveMgr::sInstance) {
        ArchivesHolder* common=ArchiveMgr::sInstance->LoadArchive(ARCHIVE_HOLDER_COMMON,parent,nullptr);
        OS::Report("[CommonMemory] Common before kart reservation bytes=%u mounted=%u parentLargest=%u kartReservation=%u\n",
            common ? common->GetTotalMountedArchivesSize() : 0,
            common ? common->HasArchives() : false,parent->getAllocatableSize(4),size);
    }
    return EGG::ExpHeap::Create(size,parent,flags);
}
kmCall(0x80553cd0, CreatePackedKartArchiveHeap);

// Log the space recovered by the existing kartModelHeap->adjust() call,
// before the parent is adjusted to create the race heaps.
static u32 AdjustPackedKartArchiveHeap(EGG::ExpHeap* child) {
    const GameScene* scene=GameScene::GetCurrent();
    EGG::ExpHeap* parent=ExtendedPlayers::IsActive() && scene
        ? scene->archiveHeapMem1 : static_cast<EGG::ExpHeap*>(nullptr);
    const u32 before=parent ? parent->getTotalFreeSize() : 0;
    const u32 result=child->adjust();
    if(parent)OS::Report("[CommonMemory] kart adjust parentFree=%u->%u parentLargest=%u; native parent adjust follows\n",
        before,parent->getTotalFreeSize(),parent->getAllocatableSize(4));
    return result;
}
kmCall(0x805541f0, AdjustPackedKartArchiveHeap);
}
