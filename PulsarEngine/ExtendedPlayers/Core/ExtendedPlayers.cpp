#include <ExtendedPlayers/Core/Roster.hpp>
#include <ItemSlots/Runtime.hpp>
#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <ExtendedPlayers/Core/MemoryAccounting.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Input/InputManager.hpp>
#include <MarioKartWii/AI/AIManager.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/Effect/EffectMgr.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/Scene/RaceScene.hpp>
#include <MarioKartWii/Scene/RootScene.hpp>
#include <MarioKartWii/Objects/ObjectsMgr.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/UI/Page/Menu/KartSelect.hpp>
#include <core/rvl/OS/OS.hpp>
#include <core/egg/Math/Quat.hpp>

// Aliases name original functions, never the overwritten entry points.
extern "C" {
void EPOriginalAwards(Racedata*);
void EPRegisterCPUGroup(void*, KartAIController*);
void EPInitCPUGroup(void*);
void EPInitDrivingGroup(void*);
u32 EPGetDrivingGroupState(void*);
void EPOriginalEnterEnd(RaceScene*);
void EPOriginalExit(RaceScene*);
void EPOriginalCalc(GameScene*);
void EPOriginalDraw(GameScene*);
void EPOriginalOnCalc(RaceScene*);
void EPOriginalCreateInstances(RaceScene*);
void EPOriginalSceneCalcAll();
void EPOriginalSlotPostProcess(void*, void*);
void EPDriverRelease(void*);
void EPOriginalKartUpdate(Kart::Player*);
void EPSub90Register(void*, u32);
void EPSub90Delete(void*, int);
bool EPItemShock(Item::Player*);
bool EPOriginalShock(Kart::Movement*);
bool EPShockEffect(Kart::Movement*, s16, u32, u32);
bool EPIsCPUCharacterUnlocked(CharacterId);
}

namespace Pulsar { namespace ExtendedPlayers {
static bool active;
// Keep this roster unchanged until the scene has finished unloading.
static u32 activeCount = VanillaCount;
static bool ready;
static const u32 Guard = 0x45503133;

// Keep the archive holders across races; GameScene frees their buffers through AddArchivesHolder.
struct Sidecars {
    u32 head;
    RacedataPlayer records[Capacity - VanillaCount];
    u32 recordsEnd;
    Input::AIControllerHolder inputs[Capacity - VanillaCount];
    u32 inputsEnd;
    ArchivesHolder* primary[Capacity - VanillaCount];
    ArchivesHolder* secondary[Capacity - VanillaCount];
    void* drivers[Capacity - VanillaCount];
    KartAIController* ai[Capacity - VanillaCount];
    u32 tail;
    Sidecars() : head(Guard), recordsEnd(Guard), inputsEnd(Guard), tail(Guard) {
        for(u32 i = 0; i < Capacity - VanillaCount; ++i) {
            primary[i] = new ArchivesHolder(1);
            secondary[i] = new ArchivesHolder(1);
            drivers[i] = nullptr;
            ai[i] = nullptr;
        }
    }
};
struct DriverTargetState { Vec3 position; Vec3* target; s32 age; s32 remaining; };
static DriverTargetState extraDriverTargets[Capacity - VanillaCount];
static Sidecars* sidecars;
static void* extraRoutes[Capacity - VanillaCount];
static u32* extraObjects[Capacity - VanillaCount];
static u32 kartTicks[Capacity];
static u32 raceGeneration;
// Keep UI coordinates separate from RaceBalloons[12], keyed by viewport owner.
struct BalloonSidecar {
    void* owner;
    Vec3 positions[Capacity - VanillaCount];
};
static BalloonSidecar balloonSidecars[4];


// Let these AI calls see at most 12 players, then restore the count before updating the race.
class LegacyAIView {
public:
    LegacyAIView() : raceCount(Racedata::sInstance->racesScenario.playerCount),
        driverCount(DriverMgr::playerCount) {
        if(active) {
            Racedata::sInstance->racesScenario.playerCount = VanillaCount;
            DriverMgr::playerCount = VanillaCount;
        }
    }
    ~LegacyAIView() {
        Racedata::sInstance->racesScenario.playerCount = raceCount;
        DriverMgr::playerCount = driverCount;
    }
private:
    u8 raceCount, driverCount;
};

static void InvariantFailureStop() {
    // Stop here only if the player state is invalid.
    OS::DisableInterrupts();
    for(;;) { asm { nop } }
}

static void Require(bool condition, const char* description) {
    if(condition) return;
    OS::Report("[EP13] STOP: %s\n", description);
    // Stop before another lookup can read an invalid player pointer.
    InvariantFailureStop();
}

bool IsActive() { return active; }
bool IsModeEnabled(GameMode mode) {
    return Config::Enabled && ((mode == MODE_VS_RACE && Config::OfflineVS)
        || (mode == MODE_GRAND_PRIX && Config::OfflineGP));
}
bool IsExtra(u32 id) { return active && id >= VanillaCount && id < activeCount; }
static u32 ExtraIndex(u32 id) {
    if(sidecars == nullptr || id < VanillaCount || id >= activeCount) {
        OS::Report(
            "[EP13] ExtraIndex REJECTED: id=%u vanilla=%u testCount=%u active=%d sidecars=%p\n",
            id, VanillaCount, activeCount, active ? 1 : 0, sidecars
        );
        Require(false, "inactive extra ID");
    }

    return id - VanillaCount;
}
RacedataPlayer& RacePlayer(u32 id) {
    if(active && id >= VanillaCount) return sidecars->records[ExtraIndex(id)];
    return Racedata::sInstance->racesScenario.players[id];
}
static void CheckGuards() {
    Require(sidecars && sidecars->head == Guard && sidecars->recordsEnd == Guard
        && sidecars->inputsEnd == Guard && sidecars->tail == Guard, "sidecar guard");
}

static bool Eligible(const RacedataScenario& scenario) {
    if(!IsSupportedRetailRegion()) return false;
    // Accept both CPU_RACE loading scenes and DEFAULT playable scenes.
    const GameType type = scenario.settings.gametype;
    if(!IsModeEnabled(scenario.settings.gamemode)
        || (type != GAMETYPE_DEFAULT && type != GAMETYPE_CPU_RACE)
        || (scenario.settings.modeFlags & 2)
        || (scenario.settings.gamemode == MODE_VS_RACE && scenario.settings.cpuMode == CPU_NONE)
        || scenario.playerCount != VanillaCount) return false;
    const u32 locals = scenario.localPlayerCount;
    // Load every kart during the single-camera intro, even for split-screen.
    // Enabling extra racers only after reinit leaves their archives missing.
    const u32 expectedScreens = locals == 3 ? 4 : locals;
    const bool introCamera = type == GAMETYPE_CPU_RACE && scenario.screenCount == 1;
    if(locals < 1 || locals > 4 || (scenario.settings.gamemode == MODE_GRAND_PRIX && locals != 1)
        || (!introCamera && scenario.screenCount != expectedScreens)) return false;
    u32 localSlots = 0;
    for(u32 id = 0; id < VanillaCount; ++id) {
        const RacedataPlayer& player = scenario.players[id];
        if(player.playerType == PLAYER_REAL_LOCAL) {
            const u32 slot = static_cast<u32>(player.hudSlotId);
            if(slot >= locals || (localSlots & (1u << slot))) return false;
            localSlots |= 1u << slot;
        }
        else if(player.playerType != PLAYER_CPU) return false;
    }
    if(localSlots != (1u << locals) - 1) return false;
    return !System::sInstance->IsContext(PULSAR_MODE_OTT) && !System::sInstance->IsContext(PULSAR_MODE_KO);
}

// Extend the picker rules at 8083EC28/8084745C, not its twelve-entry scratch.
// Keep combinations through the intro-to-race rebuild.
static void SelectExtraCombos(const RacedataScenario& scenario) {
    static bool cached;
    static u32 cachedCount;
    static CharacterId baseCharacters[VanillaCount], extraCharacters[Capacity - VanillaCount];
    static KartId baseKarts[VanillaCount], extraKarts[Capacity - VanillaCount];
    static u32 previousRestriction;
    const u32 restriction = SectionMgr::sInstance->sectionParams->kartsDisplayType;
    bool reuse = cached && cachedCount == activeCount && restriction == previousRestriction;
    for(u32 id = 0; id < VanillaCount; ++id) {
        if(baseCharacters[id] != scenario.players[id].characterId
            || baseKarts[id] != scenario.players[id].kartId) reuse = false;
        baseCharacters[id] = scenario.players[id].characterId;
        baseKarts[id] = scenario.players[id].kartId;
    }
    Random random;
    u32 used = 0, weights[3] = {0, 0, 0};
    for(u32 id = 0; id < VanillaCount; ++id) {
        const CharacterId character = scenario.players[id].characterId;
        if(static_cast<u32>(character) < 24) used |= 1u << character;
        const s32 weight = GetCharacterWeightClass(character);
        Require(weight >= 0 && weight < 3, "CPU roster weight");
        ++weights[weight];
    }
    for(u32 id = VanillaCount; id < activeCount; ++id) {
        const u32 extra = id - VanillaCount;
        if(!reuse) {
            CharacterId candidates[24];
            u32 count = 0, leastWeightCount = Capacity;
            // Allow an extra CPU in the least-populated eligible class when count/3 quotas fill.
            for(u32 allowDuplicate = 0; allowDuplicate < 2 && !count; ++allowDuplicate) {
                // Repeat only if the license has no unused unlocked character.
                for(u32 c = 0; c < 24; ++c) {
                    if((!allowDuplicate && (used & (1u << c)))
                        || !EPIsCPUCharacterUnlocked(static_cast<CharacterId>(c))) continue;
                    const u32 weightCount = weights[GetCharacterWeightClass(static_cast<CharacterId>(c))];
                    if(weightCount > leastWeightCount) continue;
                    if(weightCount < leastWeightCount) { count = 0; leastWeightCount = weightCount; }
                    candidates[count++] = static_cast<CharacterId>(c);
                }
            }
            Require(count != 0, "extra CPU character candidates");
            extraCharacters[extra] = candidates[random.NextLimited(count)];
            KartId vehicles[12];
            count = 0;
            const u32 weight = GetCharacterWeightClass(extraCharacters[extra]);
            for(u32 k = 0; k < 12; ++k) {
                const KartId kart = kartsSortedByWeight[weight][k];
                if(IsKartUnlocked(kart, restriction)) vehicles[count++] = kart;
            }
            Require(count != 0, "extra CPU vehicle candidates");
            extraKarts[extra] = vehicles[random.NextLimited(count)];
        }
        sidecars->records[extra].characterId = extraCharacters[extra];
        sidecars->records[extra].kartId = extraKarts[extra];
        used |= 1u << extraCharacters[extra];
        ++weights[GetCharacterWeightClass(extraCharacters[extra])];
        OS::Report("[EP13] CPU combo: id=%u character=%u kart=%u restriction=%u cached=%u\n",
            id, extraCharacters[extra], extraKarts[extra], restriction, reuse);
    }
    previousRestriction = restriction;
    cached = true;
    cachedCount = activeCount;
}

static void AssignGrid(RacedataScenario& scenario) {
    u8 order[Capacity], localIds[4];
    u32 count = 0;
    const bool firstRace = scenario.settings.raceNumber == 0;
    // HUD slots identify local players. Start humans at the rear only in race one;
    // later grids use standings for every racer.
    for(u32 id = 0; id < activeCount; ++id) {
        const RacedataPlayer& player = RacePlayer(id);
        if(player.playerType == PLAYER_REAL_LOCAL) {
            localIds[player.hudSlotId] = id;
            if(firstRace) continue;
        }
        u32 at = count;
        while(at && RacePlayer(order[at - 1]).prevFinishPos > player.prevFinishPos) {
            order[at] = order[at - 1]; --at;
        }
        order[at] = id;
        ++count;
    }
    if(firstRace && !DebugFrontSpawn) {
        // Sort all CPUs together; fresh IDs 12-15 otherwise always start behind native CPUs.
        Random random;
        for(u32 remaining = count; remaining > 1; --remaining) {
            const u32 other = random.NextLimited(remaining);
            const u8 id = order[remaining - 1];
            order[remaining - 1] = order[other];
            order[other] = id;
        }
    }
    u32 humans = 0;
    for(u32 id = 0; id < activeCount; ++id) if(RacePlayer(id).playerType == PLAYER_REAL_LOCAL) ++humans;
    if(firstRace) for(u32 slot = humans; slot; --slot) order[count++] = localIds[slot - 1];
    Require(count == activeCount, "grid roster");
    for(u32 rank = 0; rank < count; ++rank) {
        const u32 id = DebugFrontSpawn ? rank : order[rank];
        RacePlayer(id).prevFinishPos = RacePlayer(id).finishPos = rank + 1;
        if(firstRace) OS::Report("[EP13] grid entrant: id=%u cpu=%u slot=%u\n",
            id, RacePlayer(id).playerType == PLAYER_CPU, rank + 1);
    }
    for(u32 slot = 0; slot < humans; ++slot)
        OS::Report("[EP13] grid: race=%u debugFront=%u P%u id=%u slot=%u extraSlot=%u\n",
            scenario.settings.raceNumber, DebugFrontSpawn, slot + 1, localIds[slot],
            RacePlayer(localIds[slot]).prevFinishPos, RacePlayer(12).prevFinishPos);
}

// Log a few initialization steps without allocating memory or walking the heaps.
static bool traceRebuild;
static bool reportedHeapBad;
static void TraceRaceHeap(const char* phase, u32 id, bool verbose);
static void TraceInit(const char* phase, const void* object = nullptr) {
    const RacedataScenario& s = Racedata::sInstance->racesScenario;
    OS::Report("[13] gen=%u course=%02x type=%u count=%u active=%u %s object=%p\n",
        raceGeneration, s.settings.courseId, s.settings.gametype,
        s.playerCount, active, phase, object);
}
static void NormalizeNativeGrid(Racedata& data) {
    RacedataScenario& scenario = data.racesScenario;
    if(scenario.playerCount != VanillaCount
        || (scenario.settings.gamemode != MODE_VS_RACE && scenario.settings.gamemode != MODE_GRAND_PRIX)
        || (scenario.settings.gametype != GAMETYPE_DEFAULT && scenario.settings.gametype != GAMETYPE_CPU_RACE)) return;
    u32 seen = 0;
    bool valid = true;
    u8 order[VanillaCount];
    for(u32 id = 0; id < VanillaCount; ++id) {
        const u32 rank = scenario.players[id].prevFinishPos;
        if(rank == 0 || rank > VanillaCount || (seen & (1u << (rank - 1)))) valid = false;
        else seen |= 1u << (rank - 1);
        u32 at = id;
        while(at && scenario.players[order[at - 1]].prevFinishPos > rank) {
            order[at] = order[at - 1]; --at;
        }
        order[at] = id;
    }
    if(valid) return;
    for(u32 rank = 0; rank < VanillaCount; ++rank) {
        RacedataPlayer& player = scenario.players[order[rank]];
        player.prevFinishPos = player.finishPos = rank + 1;
        RacedataPlayer& menu = data.menusScenario.players[order[rank]];
        menu.prevFinishPos = menu.finishPos = rank + 1;
    }
}

static void InitRace(Racedata* data) {
    const u32 selectedCount = Config::PlayerCount;
    TraceInit("InitRace BEFORE", data);
    if(traceRebuild) TraceRaceHeap("InitRace BEFORE", 0xffffffff, true);
    // Native scenario copies only have room for 12 players.
    if(active) data->racesScenario.playerCount = VanillaCount;
    active = ready = false;
    ResetResults();
    memset(kartTicks, 0, sizeof(kartTicks));
    memset(balloonSidecars, 0, sizeof(balloonSidecars));
    TraceInit("scenario copy BEFORE", data);
    data->InitRace();
    TraceInit("scenario copy AFTER", data);
    if(traceRebuild) TraceRaceHeap("scenario copy AFTER", 0xffffffff, true);
    OS::Report(
    "[EP13] probe: mode=%d type=%d flags=%x cpuMode=%d count=%d local=%d screen=%d p0=%d\n",
    data->racesScenario.settings.gamemode,
    data->racesScenario.settings.gametype,
    data->racesScenario.settings.modeFlags,
    data->racesScenario.settings.cpuMode,
    data->racesScenario.playerCount,
    data->racesScenario.localPlayerCount,
    data->racesScenario.screenCount,
    data->racesScenario.players[0].playerType
);

for(u32 i = 0; i < 12; ++i) {
    OS::Report("[EP13] probe player %u type=%d\n",
        i,
        data->racesScenario.players[i].playerType);
}
    activeCount = data->racesScenario.playerCount;
    if(selectedCount <= VanillaCount || !Eligible(data->racesScenario)) {
        // Compact invalid offline ranks before native Raceinfo indexes prevFinishPos - 1.
        NormalizeNativeGrid(*data);
        TraceInit("InitRace AFTER (not eligible)", data);
        return;
    }
    Require(selectedCount <= Capacity, "selected racer capacity");
    activeCount = selectedCount;
    TraceInit("sidecars BEFORE", sidecars);
    if(!sidecars) {
        EGG::Heap* previous = System::sInstance->heap->BecomeCurrentHeap();
        void* storage = new u8[sizeof(Sidecars)];
        Require(storage != nullptr, "sidecar allocation");
        memset(storage, 0, sizeof(Sidecars));
        sidecars = new(storage) Sidecars;
        previous->BecomeCurrentHeap();
    }
    CheckGuards();
    for(u32 i = 0; i < Capacity - VanillaCount; ++i) {
        Require(sidecars->primary[i] && sidecars->secondary[i], "archive holder allocation");
        extraDriverTargets[i].target = nullptr;
        extraDriverTargets[i].age = extraDriverTargets[i].remaining = 0;
        sidecars->drivers[i] = nullptr;
        sidecars->ai[i] = nullptr;
        sidecars->inputs[i].Init();
        RacedataPlayer& player = sidecars->records[i];
        player.hudSlotId = -1;
        player.realControllerChannel = -1;
        player.playerType = VanillaCount + i < activeCount ? PLAYER_CPU : PLAYER_NONE;
        player.team = static_cast<Team>(0);
        player.previousScore = player.score = data->racesScenario.settings.raceNumber ? player.score : 0;
        if(!data->racesScenario.settings.raceNumber) player.gpHiddenScore = 0;
        if(!data->racesScenario.settings.raceNumber)
            player.prevFinishPos = VanillaCount + i + 1;
        player.finishPos = player.prevFinishPos;
    }
    TraceInit("sidecars AFTER", sidecars);
    active = true;
    SelectExtraCombos(data->racesScenario);
    AssignGrid(data->racesScenario);
    ++raceGeneration;
    data->racesScenario.playerCount = activeCount;
    System::sInstance->nonTTGhostPlayersCount = activeCount;
    OS::Report("[EP13] roster ready: %u racers; generation=%u\n", activeCount, raceGeneration);
    TraceInit("InitRace AFTER", data);
    if(traceRebuild) TraceRaceHeap("InitRace AFTER", 0xffffffff, true);
}
kmCall(0x80553c90, InitRace);
// Reinit calls Reset/OnExit, clearing eligibility, then InitRace at 80554AB0.
// Rebuild the sidecar on both entry paths or racers above ID 11 disappear after reset.
kmCall(0x80554ab0, InitRace);

static void InitAwards(Racedata* data) {
    // The three-kart award scene scans twelve records. Put extended podium winners
    // in that view without replacing a local player.
    if(IsModeEnabled(MODE_GRAND_PRIX) && HasGPResults() && sidecars
        && data->menusScenario.settings.gamemode == MODE_GRAND_PRIX
        && data->menusScenario.settings.gametype == GAMETYPE_GP_WIN) {
        for(u32 i = 0; i < activeCount - VanillaCount; ++i) {
            const RacedataPlayer& extra = sidecars->records[i];
            if(extra.prevFinishPos == 0 || extra.prevFinishPos > 3) continue;
            for(u32 id = 1; id < VanillaCount; ++id) {
                RacedataPlayer& target = data->menusScenario.players[id];
                if(target.playerType == PLAYER_CPU && target.prevFinishPos > 3) {
                    target = extra;
                    break;
                }
            }
        }
    }
    EPOriginalAwards(data);
}
kmCall(0x80553c78, InitAwards);

static RacedataPlayer* GetPlayer(RacedataScenario* scenario, u8 id) {
    if(active && scenario == &Racedata::sInstance->racesScenario && id >= VanillaCount)
        return &RacePlayer(id);
    return &scenario->players[id];
}
kmBranch(0x8052e434, GetPlayer);
kmBranch(0x8052dd20, GetPlayer);
static s32 GetHudSlot(const Racedata* data, u8 id) {
    if(active && data == Racedata::sInstance && id >= VanillaCount) return RacePlayer(id).hudSlotId;
    return data->racesScenario.players[id].hudSlotId;
}
kmBranch(0x80531f18, GetHudSlot);

static ArchivesHolder* Archive(const ArchiveMgr* manager, u32 id, bool secondary) {
    if(active && id >= VanillaCount) {
        u32 index = ExtraIndex(id);
        return secondary ? sidecars->secondary[index] : sidecars->primary[index];
    }
    // Keep the exact binary strides; no changes to ArchiveMgr's layout.
    return reinterpret_cast<ArchivesHolder*>(reinterpret_cast<u32>(manager)
        + (secondary ? 0x158 : 8) + id * 0x1c);
}
static bool HasPrimary(const ArchiveMgr* manager, u8 id) { return Archive(manager, id, false)->HasArchives(); }
static bool HasSecondary(const ArchiveMgr* manager, u8 id) { return Archive(manager, id, true)->HasArchives(); }
kmBranch(0x805415c4, HasPrimary);
kmBranch(0x805415d4, HasSecondary);

static void SetDriver(void* manager, u8 id, void* driver) {
    if(active && id >= VanillaCount) {
        sidecars->drivers[ExtraIndex(id)] = driver;
        OS::Report("[EP13] driver %u registered: mgr=%p driver=%p slot=%p\n",
            id, manager, driver, &sidecars->drivers[ExtraIndex(id)]);
    }
    else *reinterpret_cast<void**>(reinterpret_cast<u32>(manager) + 0x10 + id * 4) = driver;
}
kmBranch(0x8078cf4c, SetDriver);
// DriverMgr arrays stop at ID 11. Both look-at paths use the sidecar.
static bool SetDriverTarget(DriverMgr* manager, Vec3* target, u32 id, s32 duration, s32 minAge) {
    u8* driver;
    Vec3** targetSlot;
    s32* age;
    s32* remaining;
    if(active && id >= VanillaCount) {
        const u32 i = ExtraIndex(id);
        driver = static_cast<u8*>(sidecars->drivers[i]);
        targetSlot = &extraDriverTargets[i].target;
        age = &extraDriverTargets[i].age;
        remaining = &extraDriverTargets[i].remaining;
    } else {
        u8* base = reinterpret_cast<u8*>(manager) + id * 4;
        driver = *reinterpret_cast<u8**>(base + 0x10);
        targetSlot = reinterpret_cast<Vec3**>(base + 0xd0);
        age = reinterpret_cast<s32*>(base + 0x100);
        remaining = reinterpret_cast<s32*>(base + 0x130);
    }
    if(*age < minAge) return false;
    *targetSlot = target;
    u32& state = *reinterpret_cast<u32*>(driver + 0x2bc);
    if(state == 0 || state == 2) {
        *reinterpret_cast<float*>(driver + 0x2b8) = 0.7f;
        state = 2;
    }
    if(state == 2) *reinterpret_cast<Vec3*>(driver + 0x2a8) = *target;
    *remaining = duration;
    return true;
}
kmBranch(0x8078da90, SetDriverTarget);
static void ForceDriverTarget(DriverMgr* manager, u32 id, Vec3* target, s32 duration) {
    SetDriverTarget(manager, target, id, duration, static_cast<s32>(0x80000000));
}
kmBranch(0x8078d980, ForceDriverTarget);

extern "C" void EPCloudDriverTarget(u32 id, Vec3* target, s32 duration, s32 minAge) {
    SetDriverTarget(DriverMgr::sInstance, target, id & 0xff, duration, minAge);
}
static bool CopyDriverTarget(DriverMgr* manager, Vec3* target, u32 id, s32 duration, s32 minAge) {
    Vec3* position;
    s32 age;
    if(active && id >= VanillaCount) {
        DriverTargetState& extra = extraDriverTargets[ExtraIndex(id)];
        position = &extra.position;
        age = extra.age;
    } else {
        position = reinterpret_cast<Vec3*>(reinterpret_cast<u8*>(manager) + 0x40 + id * 12);
        age = *reinterpret_cast<s32*>(reinterpret_cast<u8*>(manager) + 0x100 + id * 4);
    }
    if(age < minAge) return false;
    *position = *target;
    return SetDriverTarget(manager, position, id, duration, minAge);
}
kmBranch(0x8078d9e8, CopyDriverTarget);
extern "C" void EPCopySphereTarget(u32 id, Vec3* target, s32 duration, s32 minAge) {
    CopyDriverTarget(DriverMgr::sInstance, target, id & 0xff, duration, minAge);
}
// base is manager + id*4. Preserve unowned sentinel12 separately from flag0x8000.
extern "C" void* EPItemDriverLookup(void* base, const u8* item) {
    const u32 delta = reinterpret_cast<u32>(base) - reinterpret_cast<u32>(DriverMgr::sInstance);
    const bool unowned = item && (*reinterpret_cast<const u32*>(item + 0x78) & 0x8000);
    if(active && !unowned && delta >= VanillaCount * 4 && delta < activeCount * 4 && !(delta & 3))
        return sidecars->drivers[ExtraIndex(delta / 4)];
    return *reinterpret_cast<void**>(static_cast<u8*>(base) + 0x10);
}
static void UpdateDrivers(DriverMgr* manager) {
    manager->Update();
    if(!active) return;
    for(u32 i = 0; i < activeCount - VanillaCount; ++i) {
        u8* driver = static_cast<u8*>(sidecars->drivers[i]);
        if(!driver) continue;
        DriverTargetState& extra = extraDriverTargets[i];
        u32& state = *reinterpret_cast<u32*>(driver + 0x2bc);
        if(state != 0 && driver[0x2b4]) extra.age = 0;
        else extra.age = static_cast<s32>(static_cast<u32>(extra.age) + 1);
        const s32 previous = extra.remaining;
        extra.remaining = static_cast<s32>(static_cast<u32>(previous) - 1);
        if(previous > 0) {
            if(state == 2) *reinterpret_cast<Vec3*>(driver + 0x2a8) = *extra.target;
        } else if(extra.target) {
            extra.target = nullptr;
            if(state == 2) state = 0;
        }
    }
}
kmCall(0x80554be4, UpdateDrivers);

static void DestroyDrivers() {
    if(active) {
        for(u32 i = 0; i < activeCount - VanillaCount; ++i) {
            if(sidecars->drivers[i]) EPDriverRelease(sidecars->drivers[i]);
            sidecars->drivers[i] = nullptr;
        }
    }
    DriverMgr::DestroyInstance();
}
kmCall(0x80554a20, DestroyDrivers);

static AI::Manager* CreateAI() {
    RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const u8 count = scenario.playerCount;
    if(active) scenario.playerCount = VanillaCount;
    for(u32 i = 0; i < Capacity - VanillaCount; ++i) { extraRoutes[i] = nullptr; extraObjects[i] = nullptr; }
    ResetAIOverflow();
    AI::Manager* manager = AI::Manager::CreateInstance();
    scenario.playerCount = count;
    return manager;
}
kmCall(0x80554488, CreateAI);
static void RegisterAI(AI::Manager* manager, KartAIController* controller) {
    const u32 id = controller->GetPlayerIdx();
    if(active && id >= VanillaCount) {
        sidecars->ai[ExtraIndex(id)] = controller;
        // Store the extra CPU pointers outside the native arrays.
        if(manager->specialItemStruct) {
            const u32 count = *reinterpret_cast<u32*>(reinterpret_cast<u8*>(manager->specialItemStruct) + 0x178);
            Require(count < Capacity, "extended CPU group capacity");
            EPRegisterCPUGroup(manager->specialItemStruct, controller);
        }
        const u32 i = ExtraIndex(id);
        u32* playerAI = *reinterpret_cast<u32**>(reinterpret_cast<u8*>(controller) + 0x10);
        u32* driving = reinterpret_cast<u32*>(playerAI[0x144 / 4]);
        u32* route = reinterpret_cast<u32*>(driving[0x3c / 4]);
        extraRoutes[i] = route;
        u32* holder = reinterpret_cast<u32*>(manager->enemyRouteHolder);
        reinterpret_cast<u32*>(route[0x14 / 4])[1] = reinterpret_cast<u32>(holder);
        holder[0x34 / 4]++;
        // Construct the extra CPU in a valid native slot, then save it in our extra-player array.
        u32 temporary[13] = {0};
        EPSub90Register(temporary, 0);
        extraObjects[i] = reinterpret_cast<u32*>(temporary[1]);
        extraObjects[i][2] = id;
        OS::Report("[EP13] CPU registered id=%u route=%p object=%p\n", id, route, extraObjects[i]);
        return;
    }
    manager->AddKartAI(controller);
}
kmCall(0x8058fda4, RegisterAI);
static KartAIController* GetAI(const AI::Manager* manager, u8 id) {
    if(active && id >= VanillaCount) return sidecars->ai[ExtraIndex(id)];
    return manager->controllers[id];
}
kmBranch(0x80739300, GetAI);
static void InitKartAI(KartAIController* controller) {
    LegacyAIView view;
    controller->Init();
}
kmCall(0x80596000, InitKartAI);
static void InitCPUGroup(void* group) {
    AI::Manager* manager = AI::Manager::sInstance;
    const u32 count = manager->playerCount;
    // 80741A10 selects CPU groups by count minus local players; expose thirteen only at this lookup.
    if(active) manager->playerCount = activeCount;
    EPInitCPUGroup(group);
    manager->playerCount = count;
}
kmWritePointer(0x808cb49c, InitCPUGroup);
static void InitAI(AI::Manager* manager) {
    LegacyAIView view;
    manager->Init();
    if(active) {
        for(u32 id = VanillaCount; id < activeCount; ++id) {
            if(manager->specialItemStruct) {
                // Give this read-only loop one extra controller at a time.
                u32 view[0x9c / 4] = {0};
                view[0x14 / 4] = 1;
                view[0x24 / 4] = reinterpret_cast<u32>(sidecars->ai[ExtraIndex(id)]);
                EPInitDrivingGroup(view);
            }
            u32* object = extraObjects[ExtraIndex(id)];
            object[1] = reinterpret_cast<u32>(Kart::Manager::sInstance->players[id]);
            object[3] = manager->difficulty == 0 ? 25 : manager->difficulty == 1 ? 50 : 75;
        }
        if(manager->specialItemStruct) {
            u8* group = static_cast<u8*>(manager->specialItemStruct);
            const u32 cpus = *reinterpret_cast<u32*>(group + 0x178);
            Require(cpus == activeCount - Racedata::sInstance->racesScenario.localPlayerCount, "CPU roster count");
            for(u32 i = 0; i < cpus; ++i) {
                void* entry = CPUGroupEntry(group, i, false);
                Require(entry && *reinterpret_cast<void**>(static_cast<u8*>(entry) + 8), "CPU group membership");
            }
            OS::Report("[EP13] CPU manager initialized: racers=%u CPUs=%u; all group memberships valid\n", activeCount, cpus);
        }
    }
}
kmCall(0x805548bc, InitAI);
static void UpdateAI(AI::Manager* manager) {
    LegacyAIView view;
    manager->Update();
    if(active && manager->specialItemStruct) {
        const u32 state = EPGetDrivingGroupState(manager->specialItemStruct);
        for(u32 id = VanillaCount; id < activeCount; ++id) {
            u32* playerAI = *reinterpret_cast<u32**>(reinterpret_cast<u8*>(sidecars->ai[ExtraIndex(id)]) + 0x10);
            u32* driving = reinterpret_cast<u32*>(playerAI[0x144 / 4]);
            u32* vtable = reinterpret_cast<u32*>(driving[0x34 / 4]);
            // Same virtual dispatch as Manager::Update at 80739484..807394A0.
            reinterpret_cast<void (*)(void*, u32)>(vtable[0x50 / 4])(driving, state);
        }
    }
}
static void UpdateKartAI(KartAIController* controller) {
    LegacyAIView view;
    controller->Update();
}
kmCall(0x80554bd8, UpdateAI);
kmCall(0x80596778, UpdateKartAI);

// Keep the native route count; store the extra controllers separately.
extern "C" void* EPRouteAt(void* holder, u32 id) {
    if(active && id >= VanillaCount) return extraRoutes[ExtraIndex(id)];
    return reinterpret_cast<void**>(holder)[1 + id];
}
static void* GetAIObject(void* owner, u32 id) {
    if(active && id >= VanillaCount) return extraObjects[ExtraIndex(id)];
    return reinterpret_cast<void**>(owner)[1 + id];
}
kmBranch(0x80739ff4, GetAIObject);
extern "C" u32 EPThunderCount() {
    const u32 cached = DriverMgr::playerCount;
    if(active) {
        const u32 count = Item::Manager::sInstance->playerCount;
        OS::Report("[EP13] lightning targets: cached=%u live=%u\n", cached, count);
        return count;
    }
    return cached;
}
static bool TraceItemShock(Item::Player* player) {
    const bool applied = EPItemShock(player);
    if(active && player->id >= VanillaCount) {
        u32* values = *reinterpret_cast<u32**>(player);
        u32* status = reinterpret_cast<u32*>(values[1]);
        OS::Report("[EP13] lightning target=%u applied=%u status=%08x/%08x\n",
            player->id, applied, status[2], status[3]);
    }
    return applied;
}
kmCall(0x807b7cd0, TraceItemShock);

// ApplyShock rejects positions >12 before touching its duration table. Its
// count-dependent index also becomes negative for the leader at count 13.
static bool ApplyExtendedShock(Kart::Movement* movement) {
    if(!active) return EPOriginalShock(movement);
    const u32 id = movement->GetPlayerIdx();
    Require(id < activeCount, "lightning player");
    const u32 rank = Raceinfo::sInstance->players[id]->position;
    const u32 count = Kart::Manager::sInstance->playerCount;
    if(rank == 0 || rank > count) {
        OS::Report("[EP13] lightning invalid rank: id=%u rank=%u count=%u\n", id, rank, count);
        return false;
    }
    static const s16 duration[12] = {690,645,600,555,510,450,390,330,270,210,150,90};
    const s32 index = static_cast<s32>(rank) + 11 - static_cast<s32>(count);
    // Extend the 45-frame steps while preserving the last-place duration and 72-frame damage phase.
    const s16 frames = 72 + (index < 0 ? duration[0] - index * 45 : duration[index]);
    const bool applied = EPShockEffect(movement, frames, 0, 0);
    if(id >= VanillaCount) OS::Report("[EP13] lightning id=%u rank=%u count=%u frames=%d applied=%u\n",
        id, rank, count, frames, applied);
    return applied;
}
kmCall(0x80798790, ApplyExtendedShock);

static void UpdateKart(Kart::Player* player) {
    EPOriginalKartUpdate(player); // Includes DriverController::Update, 8058EEE8.
    if(!active) return;
    const u32 id = player->values->playerIdx;
    u32 slot = 0;
    while(slot < activeCount && Kart::Manager::sInstance->players[slot] != player) ++slot;
    if(slot == activeCount) return;
    const u32 tick = ++kartTicks[slot];
    if(tick != 1 && tick != 60 && tick != 300) return;
    const Vec3& pos = player->GetPosition();
    DriverController* driver = player->driver;
    ModelDirector* model = driver ? driver->driverModel : static_cast<ModelDirector*>(nullptr);
    ModelDirector* lod = driver ? driver->driverModel_lod : static_cast<ModelDirector*>(nullptr);
    const u32 clip = driver && driver->clipInfo
        ? *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(driver->clipInfo) + 0x20) : 0;
    OS::Report("[EP13] TRACE slot=%u id=%u tick=%u count=%u xyz=(%.1f,%.1f,%.1f) model=%p flags=%08x lod=%08x clip=%08x\n",
        slot, id, tick, Kart::Manager::sInstance->playerCount, pos.x, pos.y, pos.z,
        model, model ? model->bitfield : 0, lod ? lod->bitfield : 0, clip);
}
kmCall(0x805900ac, UpdateKart);

// Map ranks onto the 12 file columns: leaders use 1..10, and the last two use 11/12.
static u32 ItemRaceRow(u32 index, u32 count, u32 rows) {
    if(rows < 3 || count <= rows) return index < rows ? index : rows - 1;
    if(index >= count) index = count - 1;
    if(index >= count - 2) return rows - 2 + index - (count - 2);
    return index * (rows - 3) / (count - 3);
}
extern "C" u32 EPItemProbabilityRow(u32 index, const u32* table, const Item::ItemSlotData* slot) {
    index &= 0xff;
    if(!active) return index;
    const u32 rows = table[0];
    Require(rows != 0, "empty item probability table");
    if(table == reinterpret_cast<const u32*>(&slot->playerChances)
        || table == reinterpret_cast<const u32*>(&slot->cpuChances)) {
        const u32 column = ItemRaceRow(index, Racedata::sInstance->racesScenario.playerCount, rows);
        ItemSlots::TraceProbability(column, table, slot);
        return column;
    }
    const u32 column = index < rows ? index : rows - 1; // Special boxes have independent selectors.
    return column;
}
extern "C" u32 EPItemRouletteRow(u32 rank) {
    if(!active) return rank - 1;
    if(rank == 0) rank = 1;
    const u32 columns = ItemSlots::Columns();
    return ItemRaceRow(rank - 1, Racedata::sInstance->racesScenario.playerCount, columns ? columns : VanillaCount);
}

extern "C" u32 EPLightningEligibilityTimer(const Item::ItemSlotData* slot, const Item::Player* player) {
    const u32 timer = slot->itemSpawnTimers[0];
    if(!active) return timer;
    const u32 count = Racedata::sInstance->racesScenario.playerCount;
    if(!player || player->id >= count || count < 2) return 1;
    const u32 rank = Raceinfo::sInstance->players[player->id]->position;
    // Use the native exclusion branch without changing cooldowns, capacity or awarded items.
    return rank >= count - 1 && rank <= count ? timer : 1;
}

static void PostProcessSlots(void* slotData, void* probabilities) {
    if(ItemSlots::UsesExtendedTable(static_cast<Item::ItemSlotData*>(slotData))) return;
    u32& count = *reinterpret_cast<u32*>(reinterpret_cast<u8*>(slotData) + 0x44);
    const u32 saved = count;
    if(active && count > VanillaCount) count = VanillaCount;
    EPOriginalSlotPostProcess(slotData, probabilities);
    count = saved;
}
kmCall(0x807bb1ac, PostProcessSlots);
kmCall(0x807bb1b8, PostProcessSlots);
kmCall(0x807bb0b4, PostProcessSlots);
kmCall(0x807bb0c0, PostProcessSlots);
kmCall(0x807bb5c0, PostProcessSlots);
kmCall(0x807bb5cc, PostProcessSlots);

static void Grid(KMP::Holder<KTPT>* holder, Vec3& position, Vec3& rotation, u32 position1, u32 count) {
    if(!active || count <= VanillaCount) {
        holder->CalcCoordinates_0Indexed(position, rotation, position1 - 1, count);
        return;
    }
    Require(count == activeCount && position1 >= 1 && position1 <= count, "grid count/position");
    const u32 frontCount = count > 16 ? 0 : count - 4;
    if(position1 <= frontCount) {
        holder->CalcCoordinates_0Indexed(position, rotation, position1 - 1, VanillaCount);
        return;
    }
    // Measure the grid in KTPT coordinates; extrapolated zig-zags drift sideways.
    EGG::Quatf orientation;
    const Vec3& angles = holder->raw->rotation;
    const float radians = 0.017453292f;
    orientation.SetRPY(angles.x * radians, angles.y * radians, angles.z * radians);
    EGG::Vector3f right, back;
    const EGG::Vector3f unitRight(1.0f, 0.0f, 0.0f), unitBack(0.0f, 0.0f, -1.0f);
    orientation.RotateVector(unitRight, right);
    orientation.RotateVector(unitBack, back);
    Vec3 samples[VanillaCount], unused;
    float lateral[VanillaCount], depth[VanillaCount];
    float minX = 0.0f, maxX = 0.0f, minZ = 0.0f, maxZ = 0.0f, rear = 0.0f;
    for(u32 i = 0; i < VanillaCount; ++i) {
        holder->CalcCoordinates_0Indexed(samples[i], unused, i, VanillaCount);
        const float x = samples[i].x - holder->raw->position.x;
        const float y = samples[i].y - holder->raw->position.y;
        const float z = samples[i].z - holder->raw->position.z;
        lateral[i] = x * right.x + y * right.y + z * right.z;
        depth[i] = x * back.x + y * back.y + z * back.z;
        if(!i || lateral[i] < minX) minX = lateral[i];
        if(!i || lateral[i] > maxX) maxX = lateral[i];
        if(!i || depth[i] < minZ) minZ = depth[i];
        if(!i || depth[i] > maxZ) maxZ = depth[i];
        if(i < frontCount && (!i || depth[i] > rear)) rear = depth[i];
    }
    // Point depth toward later starters, including courses with reversed KTPT orientation.
    if(depth[VanillaCount - 1] < depth[0]) {
        back.x = -back.x; back.y = -back.y; back.z = -back.z;
        // Keep right relative to the actual forward direction, too.
        right.x = -right.x; right.y = -right.y; right.z = -right.z;
        const float oldMinX = minX;
        minX = -maxX; maxX = -oldMinX;
        for(u32 i = 0; i < VanillaCount; ++i) lateral[i] = -lateral[i];
        const float oldMin = minZ;
        minZ = -maxZ; maxZ = -oldMin;
        for(u32 i = 0; i < VanillaCount; ++i) {
            depth[i] = -depth[i];
            if(i < frontCount && (!i || depth[i] > rear)) rear = depth[i];
        }
    }
    Require(maxX > minX && maxZ > minZ, "grid basis");
    const u32 rearIndex = position1 - frontCount - 1;
    float x, z;
    if(count >= 16) {
        // Sample matching columns across the six-place/two-row repeat to ignore stagger.
        const float rowSpacing = ((depth[6] - depth[0]) + (depth[9] - depth[3])) * 0.25f;
        Require(rowSpacing > 0.0f, "compact grid row spacing");
        const float nativeWidth = maxX - minX;
        u32 columns = 4;
        while(columns > 1 && nativeWidth < (columns - 1) * Config::CompactGridMinimumSeparation)
            --columns;
        float width = nativeWidth * (count > 16 ? Config::CompactGridWidth : Config::CompactRearWidth);
        const float minimumWidth = (columns - 1) * Config::CompactGridMinimumSeparation;
        if(width < minimumWidth) width = minimumWidth;
        const u32 column = rearIndex % columns, row = rearIndex / columns;
        const float fraction = columns > 1 ? 0.5f - static_cast<float>(column) / (columns - 1) : 0.0f;
        x = (minX + maxX) * 0.5f + width * fraction;
        float rowGap = rowSpacing * Config::CompactGridRowSpacing;
        float rearGap = rowSpacing * Config::CompactRearGap;
        if(rowGap < Config::CompactGridMinimumSeparation) rowGap = Config::CompactGridMinimumSeparation;
        if(rearGap < Config::CompactGridMinimumSeparation) rearGap = Config::CompactGridMinimumSeparation;
        // At24 compress all depths from the front. At16 move only the last four.
        z = (count > 16 ? minZ : rear + rearGap)
            + rowGap * row + rowSpacing * Config::CompactGridStagger * column;
    }
    else {
        // Preserve the existing13..15 placement policy.
        const u32 column = rearIndex % 4, rearRow = rearIndex / 4;
        x = minX + (maxX - minX) * (1.0f - static_cast<float>(column) / 3.0f);
        const float pairSpacing = (maxZ - minZ) * 2.0f / (VanillaCount - 1);
        z = rear + pairSpacing * (1.0f + 2.0f * rearRow + Config::RearGridStagger * column);
    }
    position.x = samples[0].x + right.x * (x - lateral[0]) + back.x * (z - depth[0]);
    position.y = samples[0].y + right.y * (x - lateral[0]) + back.y * (z - depth[0]);
    position.z = samples[0].z + right.z * (x - lateral[0]) + back.z * (z - depth[0]);
    rotation = unused;

}
kmCall(0x80536580, Grid);

static void EnterEnd(RaceScene* scene) {
    TraceRaceHeap("OnEnterEnd BEFORE", 0xffffffff, true);
    MemoryAccounting::Report("effects BEFORE", true);
    EPOriginalEnterEnd(scene);
    TraceRaceHeap("OnEnterEnd AFTER", 0xffffffff, true);
    MemoryAccounting::Report("effects AFTER", true);
    if(!active) return;
    CheckGuards();
    Require(Kart::Manager::sInstance->playerCount == activeCount, "kart count");
    Require(Item::Manager::sInstance->playerCount == activeCount, "item count");
    Require(Effects::Mgr::sInstance->playerCount == activeCount, "effect count");
    for(u32 id = VanillaCount; id < activeCount; ++id) {
        const u32 extra = ExtraIndex(id);
        const Item::Player& item = Item::Manager::sInstance->players[id];
        Require(sidecars->drivers[extra] && sidecars->ai[extra], "extra driver/AI backing");
        Require(item.id == id && item.kartPlayer == Kart::Manager::sInstance->players[id]
            && item.model2 == sidecars->drivers[extra], "extra item backing");
        Require(static_cast<const void*>(Raceinfo::sInstance->players[id]->realControllerHolder)
            == static_cast<const void*>(&sidecars->inputs[extra]), "extra input binding");
        Require(RacePlayer(id).playerType == PLAYER_CPU && RacePlayer(id).hudSlotId == -1
            && RacePlayer(id).realControllerChannel == -1, "extra CPU ownership");
        OS::Report("[EP13] extra CPU ready: id=%u driver=%p AI=%p\n",
            id, sidecars->drivers[extra], sidecars->ai[extra]);
    }
    for(u32 i = 0; i < activeCount; ++i) {
        Require(Kart::Manager::sInstance->players[i] && Effects::Mgr::sInstance->players[i], "missing racer object");
        const u32 slot = RacePlayer(i).prevFinishPos;
        Require(slot >= 1 && slot <= activeCount
            && Raceinfo::sInstance->players[i]->position == slot
            && Raceinfo::sInstance->playerIdInEachPosition[slot - 1] == i, "initial position permutation");
        OS::Report("[EP13] player %u: kart=%p item=%p effects=%p\n", i,
            Kart::Manager::sInstance->players[i], &Item::Manager::sInstance->players[i], Effects::Mgr::sInstance->players[i]);
    }
    Require(RacePlayer(12).playerType == PLAYER_CPU && RacePlayer(12).hudSlotId == -1
        && RacePlayer(12).realControllerChannel == -1, "ID12 CPU ownership");
    OS::Report("[EP13] ID12 ownership: type=%u hud=%d controller=%d\n",
        RacePlayer(12).playerType, RacePlayer(12).hudSlotId, RacePlayer(12).realControllerChannel);
    ready = true;
    OS::Report("[EP13] STARTUP COMPLETE: %u karts; extra item/driver/input/AI backing checked\n", activeCount);
    OS::Report("[EP13] LIVE: generation=%u; continuing to scene calculation and rendering\n", raceGeneration);
}
kmWritePointer(0x808b425c, EnterEnd);
// Call the original functions in the same order, with the same arguments and return values.
static Kart::Player* TraceSetupPlayer(u8 id, KartId kart, CharacterId character, bool bike) {
    TraceRaceHeap("SetupPlayer BEFORE", id, true);
    if(active) OS::Report("[13] gen=%u SetupPlayer BEFORE id=%u kart=%u char=%u bike=%u\n",
        raceGeneration, id, kart, character, bike);
    Kart::Player* player = Kart::Manager::SetupPlayer(id, kart, character, bike);
    if(active) OS::Report("[13] gen=%u SetupPlayer AFTER id=%u player=%p\n", raceGeneration, id, player);
    return player;
}
static void TracePlayerState(const char* phase, Kart::Player* player) {
    if(!active) return;
    const Kart::Values* v = player->values;
    TraceRaceHeap(phase, v->playerIdx, false);
    OS::Report("[13] gen=%u %s id=%u kart=%u char=%u player=%p values=%p driver=%p\n",
        raceGeneration, phase, v->playerIdx, v->kart, v->character, player, v, player->driver);
}
static void TracePlayerInit(Kart::Player* player) {
    TracePlayerState("PlayerInit BEFORE", player);
    player->Init();
    TracePlayerState("PlayerInit AFTER", player);
}
static void TraceCreateModel(Kart::Player* player) {
    TracePlayerState("CreateModel BEFORE", player);
    player->CreateModel();
    TracePlayerState("CreateModel AFTER", player);
    TraceRaceHeap("CreateModel AFTER", player->values->playerIdx, true);
}
kmCall(0x8058fd78, TraceSetupPlayer);
kmCall(0x8058f778, TracePlayerInit);
kmCall(0x8058fd80, TraceCreateModel);

static ObjectsMgr* TraceCourseObjects() {
    TraceInit("course objects BEFORE");
    TraceRaceHeap("course objects BEFORE", 0xffffffff, true);
    MemoryAccounting::Report("objects BEFORE", true);
    ObjectsMgr* objects = ObjectsMgr::CreateInstance();
    TraceInit("course objects AFTER", objects);
    TraceRaceHeap("course objects AFTER", 0xffffffff, true);
    MemoryAccounting::Report("objects AFTER", true);
    return objects;
}
static void TraceItemEffects(Effects::Mgr* effects) {
    TraceInit("item effects BEFORE (player effects returned)", effects);
    TraceRaceHeap("item effects BEFORE", 0xffffffff, true);
    effects->CreateItemEffects();
    TraceInit("item effects AFTER", effects);
    TraceRaceHeap("item effects AFTER", 0xffffffff, true);
}
void BeginUIAllocationTrace();
void EndUIAllocationTrace();
static void TraceSectionLoad(SectionMgr* sections) {
    TraceInit("UI LoadSection BEFORE", sections);
    TraceRaceHeap("UI LoadSection BEFORE", 0xffffffff, true);
    MemoryAccounting::Report("UI BEFORE", true);
    BeginUIAllocationTrace();
    sections->LoadSection();
    EndUIAllocationTrace();
    TraceInit("UI LoadSection AFTER", sections);
    TraceRaceHeap("UI LoadSection AFTER", 0xffffffff, true);
    MemoryAccounting::Report("UI AFTER", true);
}
kmCall(0x805545ec, TraceCourseObjects);
kmCall(0x8055462c, TraceItemEffects);
kmCall(0x80554694, TraceSectionLoad);

static bool Readable(u32 p, u32 size);
static void CreateInstances(RaceScene* scene) {
    MemoryAccounting::Begin(raceGeneration, active && activeCount == Capacity);
    MemoryAccounting::Watch("raceMEM1", scene->structsMem1);
    MemoryAccounting::Watch("raceMEM2", scene->structsMem2);
    MemoryAccounting::Watch("system", System::sInstance->heap);
    MemoryAccounting::Watch("archivesMEM1", scene->archiveHeapMem1);
    MemoryAccounting::Watch("archivesMEM2", scene->archiveHeapMem2);
    if(RootScene::sInstance) MemoryAccounting::Watch("rootMEM2", RootScene::sInstance->expHeapGroup.heaps[1]);
    MemoryAccounting::Report("instances BEFORE", true);
    // Log mounted course/common/UI/kart buffers before sizing race heaps.
    // Include compressed copies; pointer identity avoids counting shared buffers twice.
    struct ArchiveMemoryLink { nw4r::ut::Link link; ArchivesHolder* holder; ArchiveSource source; };
    const nw4r::ut::Link* link = scene->archivesLinkList.head;
    for(u32 n=0; active && activeCount==Capacity && link && n<128; ++n) {
        if(!Readable(reinterpret_cast<u32>(link),sizeof(ArchiveMemoryLink))) break;
        const ArchiveMemoryLink* entry = reinterpret_cast<const ArchiveMemoryLink*>(
            reinterpret_cast<const u8*>(link)-scene->archivesLinkList.offset);
        const ArchivesHolder* holder=entry->holder;
        if(activeCount==Capacity && holder && Readable(reinterpret_cast<u32>(holder),sizeof(ArchivesHolder))
            && holder->archiveCount<=32 && Readable(reinterpret_cast<u32>(holder->archives),holder->archiveCount*sizeof(ArchiveFile)))
            for(u32 f=0;f<holder->archiveCount;++f) {
                const ArchiveFile& file=holder->archives[f];
                OS::Report("[MemArchive] gen=%u source=%u holder=%p file=%u status=%u mounted=%p bytes=%u heap=%p compressed=%p compressedBytes=%u dumpHeap=%p\n",
                    raceGeneration,entry->source,holder,f,file.status,file.rawArchive,file.archiveSize,file.archiveHeap,
                    file.compressedArchive,file.compressedArchiveSize,file.dumpHeap);
            }
        link=static_cast<const nw4r::ut::Link*>(link->next);
    }
    TraceInit("CreateInstances BEFORE", scene);
    TraceRaceHeap("CreateInstances BEFORE", 0xffffffff, true);
    EPOriginalCreateInstances(scene);
    TraceInit("CreateInstances AFTER", scene);
    TraceRaceHeap("CreateInstances AFTER", 0xffffffff, true);
    MemoryAccounting::Report("instances AFTER", true);
    if(active) {
        CheckGuards();
        OS::Report("[EP13] managers/effects/UI constructed; entering final startup initialization\n");
    }
}
kmWritePointer(0x808b4264, CreateInstances);
static void FinalizeSceneCalc() {
    if(active) OS::Report("[EP13] first ScnMgr::CalcAll begin\n");
    EPOriginalSceneCalcAll();
    if(active) OS::Report("[EP13] first ScnMgr::CalcAll complete\n");
}
kmCall(0x8051a9d0, FinalizeSceneCalc);

static void Calc(GameScene* scene) {
    EPOriginalCalc(scene);
}
static void Draw(GameScene* scene) {
    EPOriginalDraw(scene);
}
static void OnCalc(RaceScene* scene) { EPOriginalOnCalc(scene); }
kmWritePointer(0x808b422c, Calc);
kmWritePointer(0x808b4230, Draw);
kmWritePointer(0x808b4250, OnCalc);
static void Exit(RaceScene* scene) {
    TraceInit("Exit BEFORE", scene);
    TraceRaceHeap("Exit BEFORE", 0xffffffff, true);
    if(active) for(u32 i = 0; i < activeCount - VanillaCount; ++i) {
        if(extraObjects[i]) EPSub90Delete(extraObjects[i], 1);
        extraObjects[i] = nullptr;
    }
    TraceInit("native Exit BEFORE", scene);
    EPOriginalExit(scene);
    TraceInit("native Exit AFTER", scene);
    TraceRaceHeap("native Exit AFTER", 0xffffffff, true);
    if(!active) { TraceInit("Exit AFTER", scene); return; }
    CheckGuards();
    Racedata::sInstance->racesScenario.playerCount = VanillaCount;
    System::sInstance->nonTTGhostPlayersCount = VanillaCount;
    active = ready = false;
    // Keep holders alive for GameScene's ArchiveLinks traversal; native unmount frees their buffers.
    OS::Report("[EP13] race objects destroyed; generation=%u ID12ticks=%u; vanilla count restored for teardown\n",
        raceGeneration, kartTicks[12]);
    TraceInit("Exit AFTER", scene);
}
kmWritePointer(0x808b4254, Exit);

// All five RaceBalloons projection read/write paths use this same backing.
static Vec3* BalloonPosition(void* owner, u32 id) {
    if(!IsExtra(id)) return reinterpret_cast<Vec3*>(reinterpret_cast<u8*>(owner) + 0x20) + id;
    Require(id < activeCount && owner, "balloon owner/player");
    for(u32 i = 0; i < 4; ++i) {
        BalloonSidecar& slot = balloonSidecars[i];
        if(slot.owner == owner || !slot.owner) {
            if(!slot.owner) {
                slot.owner = owner;
                OS::Report("[EP13] balloon projection sidecar owner=%p id=%u\n", owner, id);
            }
            return &slot.positions[id - VanillaCount];
        }
    }
    Require(false, "balloon owner capacity");
    return nullptr;
}
kmBranch(0x807f1da4, BalloonPosition);
extern "C" u32 EPBalloonOffset(void* owner, u32 id) {
    return reinterpret_cast<u32>(BalloonPosition(owner, id)) - reinterpret_cast<u32>(owner) - 0x20;
}

// Return the address offset needed by the original add/load/store instruction.
extern "C" u32 EPScenarioOffset(u32 id) {
    if(active && id >= VanillaCount && id < Capacity)
        return reinterpret_cast<u32>(&RacePlayer(id)) - reinterpret_cast<u32>(Racedata::sInstance) - 0x28;

    return id * 0xf0;
}
extern "C" u32 EPPrimaryOffset(u32 id) {
    return reinterpret_cast<u32>(Archive(ArchiveMgr::sInstance, id, false)) - reinterpret_cast<u32>(ArchiveMgr::sInstance) - 8;
}
extern "C" u32 EPSecondaryOffset(u32 id) {
    return reinterpret_cast<u32>(Archive(ArchiveMgr::sInstance, id, true)) - reinterpret_cast<u32>(ArchiveMgr::sInstance) - 0x158;
}
extern "C" u32 EPInputOffset(u32 id) {
    if(active && id >= VanillaCount)
        return reinterpret_cast<u32>(&sidecars->inputs[ExtraIndex(id)]) - reinterpret_cast<u32>(Input::Manager::sInstance) - 0x3b4;
    return id * 0x180;
}
extern "C" u32 EPDriverOffset(u32 id) {
    if(active && id >= VanillaCount) {
        void** slot = &sidecars->drivers[ExtraIndex(id)];
        Require(*slot != nullptr, "item driver not registered");
        const u32 offset = reinterpret_cast<u32>(slot) - reinterpret_cast<u32>(DriverMgr::sInstance) - 0x10;
        OS::Report("[EP13] item driver %u: mgr=%p slot=%p driver=%p offset=%08x\n",
            id, DriverMgr::sInstance, slot, *slot, offset);
        return offset;
    }
    return (id & 0xff) * 4;
}
extern "C" u32 EPCapCount(u32 count) {
    return active && count > VanillaCount ? VanillaCount : count;
}



} // namespace Pulsar::ExtendedPlayers
} // namespace Pulsar

// Observe the failing UI allocation without replacing its allocator or changing heaps.
#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <MarioKartWii/UI/Layout/ControlLoader.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/MEM/MEMexpHeap.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar { namespace ExtendedPlayers {
static bool tracing;
static u32 calls;
static ControlLoader* currentLoader;
static const char* currentControl;
static const char* currentVariant;
void BeginUIAllocationTrace() { tracing = IsActive(); calls = 0; }
void EndUIAllocationTrace() { tracing = false; }

static bool Readable(u32 p, u32 size) {
    return (p >= 0x80000000 && p < 0x81800000 && size <= 0x81800000 - p)
        || (p >= 0x90000000 && p < 0x94000000 && size <= 0x94000000 - p);
}
struct HeapSnapshot {
    u32 handle, start, end, head, tail, nodes, total, largest, bad;
    u32 sample, words[4];
    const char* status;
};
static HeapSnapshot InspectHeap(EGG::Heap* heap, bool used = false) {
    HeapSnapshot s = {0};
    s.status = "bad-handle";
    const u32 h = reinterpret_cast<u32>(heap);
    if((h & 3) || !Readable(h, 0x14)) return s;
    s.handle = *reinterpret_cast<const u32*>(h + 0x10);
    if((s.handle & 3) || !Readable(s.handle, 0x50)) return s;
    const MEM::iHeapHead* raw = reinterpret_cast<const MEM::iHeapHead*>(s.handle);
    if(raw->magic != 0x45585048) { s.status = "not-EXPH"; return s; }
    s.start = reinterpret_cast<u32>(raw->startAddr);
    s.end = reinterpret_cast<u32>(raw->endAddr);
    if(s.start > s.end || !Readable(s.start, s.end - s.start)) return s;
    const MEM::iExpHeapHead* exp = reinterpret_cast<const MEM::iExpHeapHead*>(s.handle + 0x3c);
    const MEM::iExpMBlockList& list = used ? exp->usedBlocks : exp->freeBlocks;
    s.head = reinterpret_cast<u32>(list.head);
    s.tail = reinterpret_cast<u32>(list.tail);
    u32 prev = 0, node = s.head;
    while(node) {
        s.bad = node;
        if(s.nodes == 4096) { s.status = "scan-limit"; return s; }
        if((node & 3) || node < s.start || node > s.end || s.end - node < 0x10) {
            s.status = "bad-link"; return s;
        }
        s.sample = node;
        for(u32 i = 0; i < 4; ++i) s.words[i] = reinterpret_cast<const u32*>(node)[i];
        const MEM::iExpHeapMBlockHead* block = reinterpret_cast<const MEM::iExpHeapMBlockHead*>(node);
        if(block->magic != (used ? 0x5544 : 0x4652)) { s.status = "bad-magic"; return s; }
        if(reinterpret_cast<u32>(block->prevBlock) != prev) { s.status = "bad-prev"; return s; }
        if(block->blockSize > s.end - node - 0x10) { s.status = "bad-size"; return s; }
        if(block->blockSize > s.largest) s.largest = block->blockSize;
        if(block->blockSize > s.end - s.start - s.total) { s.status = "bad-total"; return s; }
        s.total += block->blockSize;
        ++s.nodes;
        prev = node;
        node = reinterpret_cast<u32>(block->nextBlock);
    }
    s.bad = 0;
    s.status = prev == s.tail ? "OK" : "bad-tail";
    return s;
}
static HeapSnapshot Snapshot(EGG::Heap* heap, bool used = false) {
    // Limit the list walk in case of a cycle, and restore interrupts before logging or allocating.
    const int irq = OS::DisableInterrupts();
    HeapSnapshot s = InspectHeap(heap, used);
    OS::RestoreInterrupts(irq);
    return s;
}
// Copy the group totals while the heap is locked; restore interrupts before logging.
static void TraceRaceHeap(const char* phase, u32 id, bool verbose) {
    if(!IsActive() && !traceRebuild) return;
    const GameScene* scene = GameScene::GetCurrent();
    if(!scene) return;
    EGG::Heap* heap = scene->structsMem1;
    const HeapSnapshot s = Snapshot(heap);
    const bool bad = s.status[0] != 'O';
    if(bad && reportedHeapBad && id != 0xffffffff) return;
    if(!verbose && !bad) return;
    if(bad) reportedHeapBad = true;
    OS::Report("[13] MEM1 gen=%u phase=%s id=%u heap=%p handle=%08x range=%08x..%08x nodes=%u total=%u maxRaw=%u status=%s bad=%08x\n",
        raceGeneration, phase, id, heap, s.handle, s.start, s.end, s.nodes, s.total, s.largest, s.status, s.bad);
    if(bad) OS::Report("[13] MEM1 free=%08x..%08x sampledHeader=%08x words=%08x %08x %08x %08x\n",
        s.head, s.tail, s.sample, s.words[0], s.words[1], s.words[2], s.words[3]);
    if(id == 0xffffffff && verbose) {
        static u32 minimumFree[2];
        static EGG::Heap* sampledHeaps[2];
        for(u32 region = 0; region < 2; ++region) {
            EGG::Heap* backing = region ? scene->structsMem2 : scene->structsMem1;
            const HeapSnapshot state = region ? Snapshot(backing) : s;
            if(state.status[0] != 'O') continue;
            if(sampledHeaps[region] != backing || !strcmp(phase, "CreateInstances BEFORE")) {
                sampledHeaps[region] = backing;
                minimumFree[region] = state.total;
            }
            if(state.total < minimumFree[region]) minimumFree[region] = state.total;
            const u32 capacity = state.end - state.start;
            OS::Report("[EPMemory] gen=%u phase=%s MEM%u heap=%p capacity=%u free=%u largest=%u used=%u peakSampled=%u\n",
                raceGeneration, phase, region + 1, backing, capacity, state.total,
                state.largest, capacity - state.total, capacity - minimumFree[region]);

        }
        if(System::sInstance && System::sInstance->heap) {
            const HeapSnapshot system = Snapshot(System::sInstance->heap);
            if(system.status[0] == 'O') OS::Report("[EPMemory] gen=%u phase=%s system=%p free=%u largest=%u\n",
                raceGeneration, phase, System::sInstance->heap, system.total, system.largest);
        }
    }
}
// Log freeAll without calling any destructor or block visitor twice.
static EGG::Heap* retiringHeap;
static bool retirementFault;
static u32 disposerCalls, freeCalls;
static MEM::HeapVisitor retirementVisitor;
extern "C" void EPDiagVisitAllocated(MEM::HeapHandle heap, MEM::HeapVisitor visitor, u32 param);

static void TraceUsedBlocks(const char* phase) {
    if(!retiringHeap) return;
    const HeapSnapshot s = Snapshot(retiringHeap, true);
    OS::Report("[13] used phase=%s nodes=%u total=%u status=%s bad=%08x header=%08x words=%08x %08x %08x %08x\n",
        phase, s.nodes, s.total, s.status, s.bad, s.sample,
        s.words[0], s.words[1], s.words[2], s.words[3]);
}
static void TraceRetirementChange(const char* phase, const HeapSnapshot& before,
    u32 object, u32 target, const u32* header) {
    if(!retiringHeap || retirementFault) return;
    const HeapSnapshot after = Snapshot(retiringHeap);
    if(before.status[0] == 'O' && after.status[0] == 'O') return;
    retirementFault = true;
    OS::Report("[13] FIRST retirement anomaly phase=%s object=%08x target=%08x before=%s after=%s disposers=%u frees=%u\n",
        phase, object, target, before.status, after.status, disposerCalls, freeCalls);
    if(header) OS::Report("[13] freed block BEFORE words=%08x %08x %08x %08x\n",
        header[0], header[1], header[2], header[3]);
    TraceRaceHeap(phase, 0xffffffff, true);
    TraceUsedBlocks(phase);
}
static void TraceDisposeObject(void* object, int flag) {
    typedef void (*Destructor)(void*, int);
    const u32 target = (*reinterpret_cast<u32**>(object))[2];
    const bool observe = retiringHeap && !retirementFault;
    HeapSnapshot before = {0};
    if(observe) { before = Snapshot(retiringHeap); ++disposerCalls; }
    reinterpret_cast<Destructor>(target)(object, flag);
    if(observe) TraceRetirementChange("destructor AFTER", before,
        reinterpret_cast<u32>(object), target, nullptr);
}
static void TraceFreeVisitor(void* block, MEM::HeapHandle heap, u32 param) {
    const bool observe = retiringHeap && !retirementFault;
    HeapSnapshot before = {0};
    u32 header[4] = {0};
    if(observe) {
        before = Snapshot(retiringHeap);
        ++freeCalls;
        const u32 address = reinterpret_cast<u32>(block) - 0x10;
        if(Readable(address, 0x10)) for(u32 i = 0; i < 4; ++i)
            header[i] = reinterpret_cast<const u32*>(address)[i];
        if((header[0] >> 16) != 0x5544 || address < before.start
            || address > before.end || before.end - address < 0x10
            || header[1] > before.end - address - 0x10)
            OS::Report("[13] invalid used block BEFORE free #%u block=%p words=%08x %08x %08x %08x\n",
                freeCalls, block, header[0], header[1], header[2], header[3]);
    }
    retirementVisitor(block, heap, param);
    if(observe) TraceRetirementChange("block free AFTER", before,
        reinterpret_cast<u32>(block), reinterpret_cast<u32>(retirementVisitor), header);
}
static void TraceVisitAllocated(MEM::HeapHandle heap, MEM::HeapVisitor visitor, u32 param) {
    const bool observe = retiringHeap
        && *reinterpret_cast<MEM::HeapHandle*>(reinterpret_cast<u32>(retiringHeap) + 0x10) == heap;
    if(!observe) { EPDiagVisitAllocated(heap, visitor, param); return; }
    TraceRaceHeap("disposers AFTER", 0xffffffff, true);
    TraceUsedBlocks("disposers AFTER");
    const MEM::HeapVisitor previous = retirementVisitor;
    retirementVisitor = visitor;
    EPDiagVisitAllocated(heap, TraceFreeVisitor, param);
    retirementVisitor = previous;
    OS::Report("[13] retirement complete disposers=%u frees=%u anomaly=%u\n",
        disposerCalls, freeCalls, retirementFault);
}
kmCall(0x80229c84, TraceDisposeObject); // native virtual deleting destructor, r4=-1
kmCall(0x80226ee4, TraceVisitAllocated); // native freeAll's block visitor only

// Keep logging during reinit, even after OnExit clears the active flag.
static void TraceSceneReset(GameScene* scene) {
    traceRebuild = IsActive();
    reportedHeapBad = false;
    TraceRaceHeap("Reset BEFORE", 0xffffffff, true);
    scene->Reset();
    TraceRaceHeap("Reset AFTER", 0xffffffff, true);
}
static void TraceSceneFreeAll(EGG::ExpHeap* heap) {
    if(traceRebuild) {
        OS::Report("[13] reinit freeAll BEFORE heap=%p\n", heap);
        TraceRaceHeap("freeAll BEFORE", 0xffffffff, true);
    }
    EGG::Heap* previous = retiringHeap;
    if(traceRebuild && GameScene::GetCurrent()->structsMem1 == heap) {
        retiringHeap = heap;
        retirementFault = false;
        disposerCalls = freeCalls = 0;
        TraceUsedBlocks("freeAll BEFORE");
    }
    heap->freeAll();
    MemoryAccounting::BulkReset(heap);
    retiringHeap = previous;
    if(traceRebuild) {
        OS::Report("[13] reinit freeAll AFTER heap=%p\n", heap);
        TraceRaceHeap("freeAll AFTER", 0xffffffff, true);
        MemoryAccounting::Report("freeAll AFTER", true);
    }
}
static void TraceRebuildFinalize(GameScene* scene) {
    if(traceRebuild) TraceRaceHeap("FinalizeEnter BEFORE", 0xffffffff, true);
    scene->FinalizeEnter();
    if(traceRebuild) TraceRaceHeap("FinalizeEnter AFTER", 0xffffffff, true);
    traceRebuild = false;
}
kmCall(0x8051b7cc, TraceSceneReset);
kmCall(0x8051b7e4, TraceSceneFreeAll);
kmCall(0x8051b8b0, TraceRebuildFinalize);

static const char* SafeName(const char* name) {
    return Readable(reinterpret_cast<u32>(name), 32) ? name : "<unreadable>";
}
static void PrintAllocation(const char* phase, u32 number, u32 size, EGG::Heap* heap,
                            int align, void* result, const HeapSnapshot& s) {
    u32 groups = 0xffffffff, animations = 0xffffffff, magic = 0;
    if(currentLoader && Readable(reinterpret_cast<u32>(currentLoader->animSubHeader), 8)) {
        groups = currentLoader->animSubHeader->groupsCount;
        animations = currentLoader->animSubHeader->animationsCount;
    }
    if(currentLoader && Readable(reinterpret_cast<u32>(currentLoader->brctrRawFile), 4))
        magic = *reinterpret_cast<const u32*>(currentLoader->brctrRawFile);
    OS::Report("[13] UIalloc %s n=%u ctr=%.32s variant=%.32s groups=%u anims=%u magic=%08x bytes=%u align=%d result=%p\n",
        phase, number, SafeName(currentControl), SafeName(currentVariant), groups, animations, magic, size, align, result);
    OS::Report("[13] UIheap heap=%p handle=%08x range=%08x..%08x free=%08x..%08x nodes=%u total=%u maxRaw=%u status=%s bad=%08x\n",
        heap, s.handle, s.start, s.end, s.head, s.tail, s.nodes, s.total, s.largest, s.status, s.bad);
}
static void* TraceAnimationAllocation(u32 size, EGG::Heap* heap, int align) {
    if(!tracing) return operator new[](size, heap, align);
    const u32 number = ++calls;
    const HeapSnapshot before = Snapshot(heap);
    const bool verbose = number <= 8 || before.status[0] != 'O' || size > before.largest;
    if(verbose) PrintAllocation("BEFORE", number, size, heap, align, nullptr, before);
    void* result = operator new[](size, heap, align); // Call the original allocation function, including its null-on-failure behavior.
    if(verbose || !result) PrintAllocation("AFTER", number, size, heap, align, result, Snapshot(heap));
    return result;
}
static void TraceLoadAnimations(ControlLoader* loader, const char** names, const char* control, const char* variant) {
    if(!tracing) { loader->LoadAnimations(names, control, variant); return; }
    ControlLoader* previous = currentLoader;
    const char* previousControl = currentControl;
    const char* previousVariant = currentVariant;
    currentLoader = loader; currentControl = control; currentVariant = variant;
    loader->LoadAnimations(names, control, variant);
    currentLoader = previous; currentControl = previousControl; currentVariant = previousVariant;
}
kmCall(0x805c2d8c, TraceLoadAnimations);
kmCall(0x805c2f68, TraceAnimationAllocation);
} }
