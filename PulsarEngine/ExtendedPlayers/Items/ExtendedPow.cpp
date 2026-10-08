#include <ExtendedPlayers/ExtendedPlayers.hpp>

namespace Pulsar { namespace ExtendedPlayers {
// Leave room before the scratch array for native +9 accesses; do not change the game's object layout.
struct PowOverflow { u8 padding[9]; u8 affected[Capacity - VanillaCount]; };
static PowOverflow powOverflow;
extern "C" void* EPPowFlagBase(void* manager, u32 playerId) {
    // DeployPow initializes all active flags; this scratch needs no heap cleanup.
    if(IsActive() && playerId >= VanillaCount && playerId < Capacity)
        return reinterpret_cast<u8*>(&powOverflow) + playerId - VanillaCount;
    return static_cast<u8*>(manager) + playerId;
}
} }
