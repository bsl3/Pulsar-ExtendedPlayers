#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Race/RaceInfo/GameModeData.hpp>
#include <core/rvl/OS/OS.hpp>

extern "C" void EPOriginalPositionTracking(GMData*);

namespace Pulsar { namespace ExtendedPlayers {

// 805336D8 has double keys[12] at sp+0x18; rank 13 overwrites conversion scratch at sp+0x78.
// Use Capacity-sized scratch, keeping native finish keys and stable descending order.
static void TrackPositions(GMData* mode) {
    if(!IsActive()) { EPOriginalPositionTracking(mode); return; }
    Raceinfo& info = *mode->raceinfo;
    const u32 count = Racedata::sInstance->racesScenario.playerCount;
    if(count == 0 || count > Capacity) {
        OS::Report("[EP13] ranking invalid count=%u\n", count);
        return;
    }
    u8 order[Capacity];
    double keys[Capacity];
    u32 used = 0, filled = 0;
    // Preserve prior order for ties; validate the permutation before indexing it.
    for(u32 rank = 0; rank < count; ++rank) {
        const u32 id = info.playerIdInEachPosition[rank];
        if(id < count && !(used & (1u << id))) {
            order[filled++] = id;
            used |= 1u << id;
        }
    }
    if(filled != count) {
        OS::Report("[EP13] ranking repaired permutation: unique=%u count=%u\n", filled, count);
        for(u32 id = 0; id < count; ++id) if(!(used & (1u << id))) order[filled++] = id;
    }
    for(u32 rank = 0; rank < count; ++rank) {
        const u32 id = order[rank];
        const RaceinfoPlayer& player = *info.players[id];
        double key = player.raceCompletion;
        if(player.stateFlags & 2) {
            const Timer& timer = *player.raceFinishTime;
            const bool overflow = timer.minutes > 99;
            const u32 minutes = overflow ? 99 : timer.minutes;
            const u32 seconds = overflow ? 59 : timer.seconds;
            const u32 milliseconds = overflow ? 999 : timer.milliseconds;
            // Exact constants/ID tie-break from 80890160 and 80890168.
            key = 6000099.0 - ((minutes * 60 + seconds) * 1000 + milliseconds)
                + static_cast<double>(0.01f) * id;
        }
        keys[rank] = key;
    }
    for(u32 i = 1; i < count; ++i) {
        const double key = keys[i];
        const u8 id = order[i];
        u32 j = i;
        while(j && keys[j - 1] < key) {
            keys[j] = keys[j - 1]; order[j] = order[j - 1]; --j;
        }
        keys[j] = key; order[j] = id;
    }
    for(u32 rank = 0; rank < count; ++rank) {
        info.players[order[rank]]->position = rank + 1;
        info.playerIdInEachPosition[rank] = order[rank];
    }
    LogCPUParity();
    if(info.raceFrames && info.raceFrames % 600 == 0) {
        OS::Report("[EP13] ranking: frame=%u first=%u human=%u progress=%.5f cp=%u ID12=%u progress=%.5f cp=%u\n",
            info.raceFrames, order[0], info.players[0]->position, info.players[0]->raceCompletion,
            info.players[0]->checkpoint, info.players[12]->position, info.players[12]->raceCompletion,
            info.players[12]->checkpoint);
        for(u32 id = VanillaCount + 1; id < count; ++id)
            OS::Report("[EP13] ranking extra: frame=%u id=%u rank=%u progress=%.5f cp=%u\n",
                info.raceFrames, id, info.players[id]->position, info.players[id]->raceCompletion,
                info.players[id]->checkpoint);
    }
}
// GP and VS share the same native routine and extended ranking implementation.
kmWritePointer(0x808b3434, TrackPositions);
kmWritePointer(0x808b3460, TrackPositions);
} }
