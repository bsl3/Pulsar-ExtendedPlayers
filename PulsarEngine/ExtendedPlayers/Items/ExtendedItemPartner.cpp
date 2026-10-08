#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>

namespace Pulsar { namespace ExtendedPlayers {
extern "C" Item::Player* EPItemRoulettePartner(void* indexedManager) {
    Item::Manager* manager = Item::Manager::sInstance;
    const u32 delta = reinterpret_cast<u32>(indexedManager) - reinterpret_cast<u32>(manager);
    // This table stores local online partners, not players. Extra offline CPUs have none;
    // index14 would read ObjHolder[0].capacity at manager+0x50 as a pointer.
    if(IsActive() && delta >= VanillaCount * 4 && delta < Capacity * 4 && !(delta & 3))
        return nullptr;
    return *reinterpret_cast<Item::Player**>(static_cast<u8*>(indexedManager) + 0x18);
}
} }
