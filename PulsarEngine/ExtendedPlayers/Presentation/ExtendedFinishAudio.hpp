#ifndef _PUL_EXTENDED_FINISH_AUDIO_
#define _PUL_EXTENDED_FINISH_AUDIO_
// Included by ExtendedResults.cpp to avoid adding another object to the linker command line.
#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <ExtendedPlayers/Config.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

extern "C" FinishType EPVanillaFinishType(u8 playerId);
namespace Pulsar { namespace ExtendedPlayers {
static_assert(Config::HappyFinishDenominator > 0, "finish threshold denominator");
static_assert(Config::HappyFinishNumerator > 0 &&
    Config::HappyFinishNumerator <= Config::HappyFinishDenominator, "finish threshold fraction");
static FinishType FinishAudioType(u8 playerId) {
    if(!IsActive()) return EPVanillaFinishType(playerId);
    const u32 count = Racedata::sInstance->racesScenario.playerCount;
    if(count <= VanillaCount || count > Capacity || playerId >= count)
        return EPVanillaFinishType(playerId);
    const u32 place = Raceinfo::sInstance->players[playerId]->position;
    const u32 happy = (count * Config::HappyFinishNumerator + Config::HappyFinishDenominator - 1)
        / Config::HappyFinishDenominator;
    // Keep the existing first-place victory fanfare; reuse good/bad sounds.
    if(place == 1) return FINISH_TYPE_FIRST;
    return place && place <= happy ? FINISH_TYPE_GOOD : FINISH_TYPE_BAD;
}
// Use one finish classification for fanfare, animation and voice; inactive modes stay native.
kmCall(0x807121fc, FinishAudioType);
kmCall(0x8071223c, FinishAudioType);
kmCall(0x80712250, FinishAudioType);
kmCall(0x80712270, FinishAudioType);
kmCall(0x807122c4, FinishAudioType);
kmCall(0x80712364, FinishAudioType);
kmCall(0x80712390, FinishAudioType);
kmCall(0x807123b0, FinishAudioType);
kmCall(0x807cc7f0, FinishAudioType);
kmCall(0x807cc880, FinishAudioType);
kmCall(0x808644b0, FinishAudioType);
} }

#endif
