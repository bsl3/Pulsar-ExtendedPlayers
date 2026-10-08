#ifndef _PUL_EXTENDED_PLAYERS_
#define _PUL_EXTENDED_PLAYERS_
#include <MarioKartWii/Race/Racedata.hpp>
#include <ExtendedPlayers/Config.hpp>
#include <ExtendedPlayers/Core/Roster.hpp>

class LayoutUIControl;
namespace Pulsar { namespace ExtendedPlayers {
// Set true and rebuild to put the human first and extra racers behind the grid.
static const bool DebugFrontSpawn = false;
void LogCPUParity();
void ResetResults();
void ResetAIOverflow();
void*& CPUGroupEntry(void* owner, u32 index, bool sorted);
bool IsActive();
bool IsModeEnabled(GameMode mode);
bool HasGPResults();
u32 SavedResultCount();
RacedataPlayer& SavedResultPlayer(u32);
void LayoutAwardRow(LayoutUIControl*, u32, bool initial=true);
u32 PlacementPointsForCount(u32 count, u32 place);
bool IsExtra(u32 id);
RacedataPlayer& RacePlayer(u32 id);
} }
#endif
