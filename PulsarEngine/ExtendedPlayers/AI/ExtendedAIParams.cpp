#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <MarioKartWii/AI/AIParams.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/AI/AIManager.hpp>
#include <MarioKartWii/AI/CPUDriving.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <core/rvl/OS/OS.hpp>

extern "C" u32 EPOriginalBestCPUScore(AI::Manager*, bool);

namespace Pulsar { namespace ExtendedPlayers {
// Compare CPU snapshots within a group; native balancing differs between groups.
void LogCPUParity() {
    if(!Config::CPUParityLogging || !IsActive()) return;
    const Raceinfo& race = *Raceinfo::sInstance;
    if(race.raceFrames != 60 && race.raceFrames != 600
        && race.raceFrames != 1800 && race.raceFrames != 3600) return;
    AI::Manager* manager = AI::Manager::sInstance;
    const u32 count = Kart::Manager::sInstance->playerCount;
    const AI::Params::ParamSpeed* shared = manager->params->speed;
    OS::Report("[EP13] AI params: frame=%u cc=%u difficulty=%u values=%.3f/%.3f/%.3f\n",
        race.raceFrames, manager->engineClass, manager->difficulty,
        shared->speedAdvantage, shared->speedBias, shared->baseSpeed);
    for(u32 id = 0; id < count; ++id) {
        if(RacePlayer(id).playerType != PLAYER_CPU) continue;
        KartAIController* controller = manager->GetKartAIController(id);
        const u8* player = reinterpret_cast<const u8*>(controller->playerAI);
        const u32* speed = *reinterpret_cast<const u32* const*>(player + 0x148);
        const u8* driving = *reinterpret_cast<const u8* const*>(player + 0x144);
        const u32* tracker = *reinterpret_cast<const u32* const*>(driving + 0x8c);
        Kart::Movement* movement = &Kart::Manager::sInstance->players[id]->GetMovement();
        OS::Report("[EP13] AI parity: frame=%u id=%u cpu=%u grid=%u rank=%u cp=%u progress=%.5f speedVT=%08x shared=%u group=%p base=%.2f target=%.2f speed=%.2f accel=%.3f route=%p\n",
            race.raceFrames, id, controller->IsCPU(), RacePlayer(id).prevFinishPos,
            race.players[id]->position, race.players[id]->checkpoint, race.players[id]->raceCompletion,
            speed[0], speed[2] == reinterpret_cast<u32>(shared), reinterpret_cast<void*>(tracker[2]),
            movement->baseSpeed, movement->unknown_0x1c, movement->engineSpeed,
            movement->acceleration, *reinterpret_cast<const void* const*>(driving + 0x3c));
        const AI::EnemyRouteController* route = *reinterpret_cast<const AI::EnemyRouteController* const*>(driving + 0x3c);
        if(route && route->enptController) {
            const AI::ENPTController& enpt = *route->enptController;
            OS::Report("[EP13] AI route: frame=%u id=%u cur=%u next=%u prev=%u width=%.2f offset=%.2f dist=%.2f difficulty=%u groupRank=%u relative=%.2f ramp=%.3f\n",
                race.raceFrames, id, enpt.curENPT, enpt.nextENPT, enpt.prevENPT,
                enpt.curENPTWidth, enpt.nextPointOffset, route->distToNextPoint,
                route->difficulty, tracker[0x14 / 4],
                *reinterpret_cast<const float*>(reinterpret_cast<const u8*>(tracker) + 0x24),
                *reinterpret_cast<const float*>(reinterpret_cast<const u8*>(speed) + 0x18));
        }
    }
}

// These eight callers pass position - 1, not player ID (8073EF04..8073F368).
static AI::ParamAction* GetAction(const AI::Params* params, u8 rank) {
    if(IsActive()) {
        const u32 count = Kart::Manager::sInstance->playerCount;
        u32 index = rank < count ? rank : count - 1;
        if(count > VanillaCount) index = index * (VanillaCount - 1) / (count - 1);
        return &params->actions[index];
    }
    return &params->actions[rank];
}
kmBranch(0x8073ac88, GetAction);

static u32 BestPreviousScore(AI::Manager* manager, bool cpu) {
    if(!IsActive()) return EPOriginalBestCPUScore(manager, cpu);
    u32 best = 0;
    const u32 count = Kart::Manager::sInstance->playerCount;
    for(u32 id = 0; id < count; ++id) {
        const RacedataPlayer& player = RacePlayer(id);
        if(player.playerType == (cpu ? PLAYER_CPU : PLAYER_REAL_LOCAL)
            && player.previousScore > best) best = player.previousScore;
    }
    return best;
}
// Native CPU-group/catch-up selection compares best human and CPU series scores.
kmCall(0x80739a98, BestPreviousScore);
kmCall(0x80739aa8, BestPreviousScore);
} }
