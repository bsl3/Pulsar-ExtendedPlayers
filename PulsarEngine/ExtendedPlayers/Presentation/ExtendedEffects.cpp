#include <ExtendedPlayers/Core/Roster.hpp>
#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <MarioKartWii/Effect/EffectMgr.hpp>
#include <core/egg/Effect/EffectCreator.hpp>

namespace Pulsar { namespace ExtendedPlayers {
enum { NativeCreators = 16, CreatorCapacity = NativeCreators + Capacity - VanillaCount };
static EGG::EffectManager* CreateManager(u32 count, EGG::Heap* heap) {
    // Reserve the extra effect creators before we know the racer count; unused slots do not affect normal races.
    if(IsSupportedRetailRegion() && Config::Enabled
        && (Config::OfflineVS || Config::OfflineGP) && count < CreatorCapacity) count = CreatorCapacity;
    return EGG::EffectManager::Create(count, heap);
}
static void InitCreators(Effects::Mgr* manager, u32 scene) {
    manager->Init(scene); // Native creates/registers slots 0..15.
    EGG::EffectManager* egg = Effects::Mgr::eggEffectMgr;
    for(u32 index = NativeCreators; index < egg->maxEffectsCount; ++index)
        new EGG::EffectCreator(index); // The scene heap owns this creator.
}
extern "C" u32 EPPlayerEffectCreator(u32 id) {
    // Vanilla players occupy2..13. Keep reserved creators 14/15 separate.
    return IsActive() && id >= VanillaCount && id < Capacity ? id + 4 : id + 2;
}
static int MapPlayerCreator(int index) {
    return index >= 2 ? EPPlayerEffectCreator(index - 2) : index;
}
static void CalcPlayer(EGG::EffectManager* manager, int creator) {
    manager->CalcEffect(MapPlayerCreator(creator));
}
static void DrawPlayer(EGG::EffectManager* manager, const nw4r::ef::DrawInfo& info, int creator, int group) {
    manager->Draw(info, MapPlayerCreator(creator), group);
}
kmCall(0x8067b530, CreateManager);
kmCall(0x8051a6c0, InitCreators);
kmCall(0x8067cd10, CalcPlayer);
kmCall(0x8067d614, DrawPlayer);
kmCall(0x8067d6a8, DrawPlayer);
kmCall(0x8067d8b4, DrawPlayer);
} }
