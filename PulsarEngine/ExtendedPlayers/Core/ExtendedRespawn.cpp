#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar { namespace ExtendedPlayers {
// 805189BC indexes twelve signed byte pairs at 8088FA08 by playerId.
// ID 12 instead reads 42 48 (the next float, 50.0): +9900 sideways and
// +21600 along the respawn basis, placing the kart far outside the route.
static void RespawnPosition(KMP::Holder<JGPT>* holder, Vec& dest, u8 id) {
    const GameType type = Racedata::sInstance->racesScenario.settings.gametype;
    if(!IsActive() || id < VanillaCount || id >= Capacity
        || type == GAMETYPE_CPU_RACE || type == static_cast<GameType>(2)) {
        holder->GetPosition(dest, id);
        return;
    }
    // Continue native rows: (-1,+1,-3,+3)*150 across and row*300 along JGPT z. IDs 0..11 stay native.
    holder->GetPosition(dest, id % 4);
    const float offset = 300.0f * (id / 4);
    dest.x += holder->zScale.x * offset;
    dest.y += holder->zScale.y * offset;
    dest.z += holder->zScale.z * offset;
    OS::Report("[EP13] respawn: id=%u xyz=(%.1f,%.1f,%.1f)\n", id, dest.x, dest.y, dest.z);
}
// The other native caller (8053913C) uses sentinel 255, never a racer ID.
kmCall(0x80584388, RespawnPosition);
} }
