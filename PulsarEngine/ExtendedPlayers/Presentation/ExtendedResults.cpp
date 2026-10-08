#include <ExtendedPlayers/Core/Roster.hpp>
#include <ExtendedPlayers/Presentation/ExtendedFinishAudio.hpp>
#include <ExtendedPlayers/Presentation/ExtendedResultBank.hpp>
#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <ExtendedPlayers/Config.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/GPVSLeaderboardTotal.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <core/egg/mem/ExpHeap.hpp>
#include <UI/UI.hpp>
#include <core/System/SystemManager.hpp>
#include <core/rvl/OS/OS.hpp>

extern "C" {
void EPComputeGPRank(RaceinfoPlayer*);
void EPResultLoad(CtrlRaceResult*);
void EPResultResetPosition(UIControl*);
void EPResultFill(CtrlRaceResult*, u8, u8);
void EPResultFillScore(CtrlRaceResult*, u32, u32);
void EPResultAnimateScore(CtrlRaceResult*, u32, bool);
void EPResultUpdateRows(Pages::GPVSLeaderboardUpdate*);
void EPResultTotalRows(Pages::GPVSLeaderboardTotal*);
}
namespace Pulsar { namespace ExtendedPlayers {
static bool scored;
static bool gpResults;
static u32 resultCount;
static u16 before[Capacity], totals[Capacity];
static u8 order[Capacity];
// Save the results before the ceremony rebuilds the racers; do not keep scene pointers.
static u32 savedRecords[Capacity][sizeof(RacedataPlayer) / sizeof(u32)];
u32 SavedResultCount() { return scored ? resultCount : 0; }
RacedataPlayer& SavedResultPlayer(u32 id) { return *reinterpret_cast<RacedataPlayer*>(savedRecords[id]); }
static PositionAndScale firstRow[4], rowStep[4];
// Save the original row sizes; clear the row pointers when a race starts.
struct ResultRowState {
    CtrlRaceResult* row;
    PositionAndScale nativeBase;
    const Config::ResultLayout* applied;
    float backgrounds[5], edges[2], textHeights[7];
    Vec2 pictures[2], fonts[7];
};
static ResultRowState resultRows[2][Capacity];
void CopyResultBankMetrics() {
    // Use the original row sizes so shared rows are not scaled twice.
    memcpy(resultRows[1],resultRows[0],sizeof(resultRows[0]));
}
void ResetResults() {
    // Keep completed values through the three-actor ceremony rebuild, but discard old scene pointers.
    const bool ceremony = gpResults && Racedata::sInstance->menusScenario.settings.gametype == GAMETYPE_GP_WIN;
    if(!ceremony) { scored = false; gpResults = false; resultCount = 0; }
    memset(resultRows, 0, sizeof(resultRows));
}
bool HasGPResults() { return scored && gpResults; }

static bool UsesVSScoring() {
    if(IsActive()) return true;
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    return IsSupportedRetailRegion()
        && scenario.playerCount == VanillaCount
        && scenario.settings.gamemode == MODE_VS_RACE && IsModeEnabled(MODE_VS_RACE)
        && (scenario.settings.gametype == GAMETYPE_DEFAULT || scenario.settings.gametype == GAMETYPE_CPU_RACE)
        && !(scenario.settings.modeFlags & 2)
        && !System::sInstance->IsContext(PULSAR_MODE_OTT)
        && !System::sInstance->IsContext(PULSAR_MODE_KO);
}

// One-based place: descending one-point steps, with the requested podium bonuses.
u32 PlacementPointsForCount(u32 count, u32 place) {
    if(!count || count > Capacity || !place || place > count) return 0;
    if(count < VanillaCount) return Racedata::pointsRoom[count - 1][place - 1];
    static const u8 podiumBonus[3] = {3, 1, 0};
    const u32 bonus = place <= 3 ? podiumBonus[place - 1] + (count >= 16 ? 1 : 0) : 0;
    return count - place + 1 + bonus;
}

static const char* resultBackgrounds[] = {
    "select_gold_01", "select_gold_02", "select_base", "team_color_c", "grade"
};
static const char* resultEdges[] = {"line", "hight_light"};
static const char* resultPictures[] = {"chara_icon", "chara_icon_sha"};
static const char* resultTexts[] = {
    "get_point", "total_score", "total_point", "position", "handle_text", "time", "player_name"
};
static u32 ResultRowsPerColumn() {
    return Config::ResultRowsPerColumn(Racedata::sInstance->racesScenario.playerCount);
}
static const Config::ResultLayout& ResultLayoutForCount(u32 count) {
    const bool larger = Config::LargeResults;
    const Config::ResultLayout& base = count <= 16
        ? (larger ? Config::LargerResultLayout : Config::DefaultResultLayout)
        : (larger ? Config::LargerResult24Layout : Config::DefaultResult24Layout);
    const u32 rows = Config::ResultRowsPerColumn(count);
    const u32 nativeRows = count <= 16 ? 8 : 12;
    if(rows == nativeRows) return base;
    // Cache a layout for each KO roster size; do not allocate rows for absent racers.
    static Config::ResultLayout intermediate[2][5]; //7..11 rows, including compact18 (nine)
    Config::ResultLayout& layout = intermediate[larger ? 1 : 0][rows - 7];
    layout = base;
    const float factor = static_cast<float>(nativeRows) / rows;
    layout.rowHeight *= factor;
    layout.rowSpacing *= factor;
    layout.topY = (rows - 1) * layout.rowSpacing * 0.5f;
    return layout;
}
static const Config::ResultLayout& SelectedResultLayout() {
    return ResultLayoutForCount(Racedata::sInstance->racesScenario.playerCount);
}
void LayoutAwardRow(LayoutUIControl* row, u32 index, bool initial) {
    const u32 count = SavedResultCount();
    if(count <= VanillaCount) return;
    const Config::ResultLayout& config = ResultLayoutForCount(count);
    const u32 rows = Config::ResultRowsPerColumn(count);
    const float fit = SystemManager::sInstance->isWideScreen ? 1.0f : config.aspectFit43;
    PositionAndScale& base = row->positionAndscale[0];
    base.position.x = config.columnX[index / rows] * fit;
    base.position.y = ((config.topY - (index % rows) * config.rowSpacing) * 0.90f - 12.0f) * fit;
    // Award rows are262 units wide; race rows are602. Fit their contents separately.
    const float widthRatio = 602.0f / 262.0f;
    const float height = 34.0f * config.rowHeight / widthRatio;
    base.scale.x = config.rowScale * fit * widthRatio * 0.90f;
    base.scale.z = config.rowScale * fit * widthRatio * 0.90f;
    static const char* backgrounds[] = {"gold_parts", "team_color_c", "grade", "p_color_r"};
    lyt::Pane* colorAnchor=row->layout.GetPaneByName("p_color_null");
    if(colorAnchor) {
        colorAnchor->trans.x=colorAnchor->trans.y=0.0f;
        colorAnchor->scale.x=colorAnchor->scale.z=1.0f;
        colorAnchor->rotate.x=colorAnchor->rotate.y=colorAnchor->rotate.z=0.0f;
    }
    for(u32 i = 0; i < 4; ++i) {
        lyt::Pane* pane = row->layout.GetPaneByName(backgrounds[i]);
        if(pane) { pane->size.x = 262.0f; pane->size.z = height; pane->trans.x = pane->trans.y = 0.0f;
            pane->origin=4; // centered, including the native right-anchored orange pane
            pane->scale.x=pane->scale.z=1.0f;
            if(i==3)pane->rotate.x=pane->rotate.y=pane->rotate.z=0.0f;
        }
    }
    for(u32 i = 0; i < 2; ++i) {
        lyt::Pane* pane = row->layout.GetPaneByName(i ? "hight_light" : "line");
        if(pane) {
            pane->size.x = 262.0f;
            pane->trans.x = i ? 0.0f : -131.0f;
            pane->trans.y = (i ? 1.0f : -1.0f) * (height - pane->size.z) * 0.5f;
        }
    }
    // Native text and portraits are 38.25 and 37.8 units tall. Leave four units for the borders.
    const float fontRoom = (34.0f * config.rowHeight - 4.0f) / 38.25f;
    const float portraitRoom = (34.0f * config.rowHeight - 4.0f) / 37.8f;
    const float content = (config.contentScale < fontRoom ? config.contentScale : fontRoom) / widthRatio;
    const float portrait = (config.portraitScale < portraitRoom ? config.portraitScale : portraitRoom) / widthRatio;
    static const char* texts[] = {"position", "mii_name"};
    for(u32 i = 0; i < 2; ++i) {
        lyt::TextBox* text = static_cast<lyt::TextBox*>(row->layout.GetPaneByName(texts[i]));
        if(text) {
            text->fontSizeX = (i ? 32.25f : 23.1f) * content;
            text->fontSizeY = (i ? 38.25f : 28.05f) * content;
            text->size.z = height; text->trans.y = 0.0f;
            text->scale.x = i ? content : 0.9f * content;
            text->scale.z = content;
        }
    }
    // MKW caches glyph geometry; pane scaling shrinks text even after SetMessage.
    lyt::Pane* nameAnchor=row->layout.GetPaneByName("mii_name_null");
    if(nameAnchor) {
        nameAnchor->scale.x=nameAnchor->scale.z=0.18f;
        nameAnchor->trans.x=13.333335f*(1.0f-0.18f);
        nameAnchor->trans.y=0.0f;
    }
    lyt::Pane* points = row->layout.GetPaneByName("Null_00");
    if(points) { points->scale.x = content; points->scale.z = content; points->trans.y = 0.0f; }
    lyt::Pane* picture = row->layout.GetPaneByName("chara_icon_null");
    if(picture) {
        picture->scale.x = 0.9f * portrait; picture->scale.z = 0.9f * portrait;
        picture->trans.y = -3.09f * picture->scale.z; // authored icon/shadow midpoint
    }
    EPResultResetPosition(row);
    // Update the child matrices after resizing so the next draw uses the new layout.
    row->layout.Update(nullptr);
}
static ResultRowState& RowState(CtrlRaceResult* row) {
    const bool total = row->parentGroup->parentPage->pageId == Pages::GPVSLeaderboardTotal::id;
    return resultRows[total ? 1 : 0][row->id - 1];
}
static void CaptureResultMetrics(CtrlRaceResult* row) {
    ResultRowState& state = RowState(row);
    state.row = row;
    state.nativeBase = row->positionAndscale[0];
    state.applied = nullptr;
    for(u32 i = 0; i < 5; ++i) {
        const lyt::Pane* pane = row->layout.GetPaneByName(resultBackgrounds[i]);
        if(pane) state.backgrounds[i] = pane->size.z;
    }
    for(u32 i = 0; i < 2; ++i) {
        const lyt::Pane* edge = row->layout.GetPaneByName(resultEdges[i]);
        const lyt::Pane* picture = row->layout.GetPaneByName(resultPictures[i]);
        if(edge) state.edges[i] = edge->trans.y;
        if(picture) { state.pictures[i].x = picture->size.x; state.pictures[i].z = picture->size.z; }
    }
    for(u32 i = 0; i < 7; ++i) {
        const lyt::TextBox* text = static_cast<lyt::TextBox*>(row->layout.GetPaneByName(resultTexts[i]));
        if(text) {
            state.fonts[i].x = text->fontSizeX; state.fonts[i].z = text->fontSizeY;
            state.textHeights[i] = text->size.z;
        }
    }
}
// BRLANs animate ranking_null, colors/alpha and gold rotation, not child sizes.
// Restore the original sizes before scaling, so repeated layout calls do not shrink the rows again.
static void ApplyResultLayout(CtrlRaceResult* row) {
    ResultRowState& state = RowState(row);
    const Config::ResultLayout& config = SelectedResultLayout();
    if(state.row != row || state.applied == &config) return;
    const float centerY = Config::ResultCenterY;
    const float height = 34.0f * config.rowHeight;
    for(u32 i = 0; i < 5; ++i) {
        lyt::Pane* pane = row->layout.GetPaneByName(resultBackgrounds[i]);
        if(pane) {
            pane->size.z = config.fillRow ? height : state.backgrounds[i] * config.rowHeight;
            pane->trans.y = centerY;
        }
    }
    for(u32 i = 0; i < 2; ++i) {
        lyt::Pane* pane = row->layout.GetPaneByName(resultEdges[i]);
        if(pane) pane->trans.y = config.fillRow
            ? centerY + (i == 0 ? -height * 0.5f : height * 0.5f - pane->size.z * 0.5f)
            : centerY + (state.edges[i] - centerY) * config.rowHeight;
    }
    lyt::Pane* portrait = row->layout.GetPaneByName("chara_icon");
    lyt::Pane* anchor = row->layout.GetPaneByName("chara_icon_null");
    if(portrait && anchor) anchor->trans.y = centerY - portrait->trans.y;
    for(u32 i = 0; i < 2; ++i) {
        lyt::Pane* pane = row->layout.GetPaneByName(resultPictures[i]);
        if(pane) {
            pane->size.x = state.pictures[i].x * config.portraitScale;
            pane->size.z = state.pictures[i].z * config.portraitScale;
        }
    }
    for(u32 i = 0; i < 7; ++i) {
        lyt::TextBox* text = static_cast<lyt::TextBox*>(row->layout.GetPaneByName(resultTexts[i]));
        if(text) {
            text->fontSizeX = state.fonts[i].x * config.contentScale;
            text->fontSizeY = state.fonts[i].z * config.contentScale;
            // Increase the text box's vertical room, not its horizontal fitting.
            text->size.z = config.fillRow ? height : state.textHeights[i];
            text->trans.y = centerY;
        }
    }
    const u32 index = row->id - 1, column = index / ResultRowsPerColumn(), rowIndex = index % ResultRowsPerColumn();
    const float fit = SystemManager::sInstance->isWideScreen ? 1.0f : config.aspectFit43;
    PositionAndScale& base = row->positionAndscale[0];
    base = state.nativeBase;
    base.position.x = config.columnX[column] * fit;
    // The visible native row is centered at x=-1.87; compensate only in Larger.
    if(config.fillRow) base.position.x += 1.87f * config.rowScale * fit;
    base.position.y = (config.topY - static_cast<float>(rowIndex) * config.rowSpacing
        - centerY * config.rowScale) * fit;
    base.scale.x *= config.rowScale * fit;
    base.scale.z *= config.rowScale * fit;
    // Rebuild [1]/[3] from [0]; leave native entrance/exit delta [2] untouched.
    EPResultResetPosition(row);
    state.applied = &config;
}

// Layouts use the selected heap; text and animation arrays use currentHeap.
// Spread the shared rows across available heaps; native delete[] finds their owner.
static void LoadNativeResult(CtrlRaceResult* row, u32 logicalRank) {
    if(Racedata::sInstance->racesScenario.playerCount != Capacity) {
        EPResultLoad(row);
        return;
    }
    const GameScene* scene = GameScene::GetCurrent();
    const u32 rowFreeBefore = scene->structsMem1->getTotalFreeSize()+scene->structsMem2->getTotalFreeSize();
    EGG::Heap* backing = scene->structsMem1->getAllocatableSize(4)
        > scene->structsMem2->getAllocatableSize(4) ? scene->structsMem1 : scene->structsMem2;
    // Native destructors find the heap that owns each text and animation array.
    // Use spare system memory but leave 512 KiB for IO/settings/tasks; do not reserve another pool.
    EGG::Heap* system = System::sInstance ? System::sInstance->heap : static_cast<EGG::Heap*>(nullptr);
    const u32 reserve = 0x80000;
    const u32 systemLargest = system ? system->getAllocatableSize(4) : 0;
    // A row needs under 64 KiB of C++ backing. Use spare system space before scene exhaustion.
    if(systemLargest >= reserve + 0x10000)
        backing = system;
    EGG::Heap* previous = backing->BecomeCurrentHeap();
    const bool report = logicalRank == 1 || logicalRank == Capacity;
    if(report) OS::Report("[EP13] result backing BEGIN rank=%u heap=%p free=%u system=%p largest=%u reserve=%u\n",
        logicalRank, backing, backing->getAllocatableSize(4), system, systemLargest, reserve);
    EPResultLoad(row);
    previous->BecomeCurrentHeap();
    RecordResultBankRowCost(logicalRank,rowFreeBefore-scene->structsMem1->getTotalFreeSize()-scene->structsMem2->getTotalFreeSize());
    if(report) OS::Report("[EP13] result backing END rank=%u free=%u restored=%p\n",
        logicalRank, backing->getAllocatableSize(4), previous);
}

// Reuse a real BRCTR variant: vanilla ResultGP provides rank 1..rank 12 only.
// Reuse native row assets, including their internal score/portrait animations.
static void LoadResult(CtrlRaceResult* row) {
    if(!IsActive()) { EPResultLoad(row); return; }
    const u8 id = row->id;
    if(Racedata::sInstance->racesScenario.playerCount > VanillaCount) {
        // Both result pages call this Load function; keep the column variants' animation delays.
        const u32 index = id - 1;
        const u32 column = index / ResultRowsPerColumn(), rowIndex = index % ResultRowsPerColumn();
        row->id = rowIndex + 1;
        LoadNativeResult(row, id);
        row->id = id;
        CaptureResultMetrics(row);
        ApplyResultLayout(row);
        const PositionAndScale& base = row->positionAndscale[0];
        if(id == 1 || id == 9 || id == 16)
            OS::Report("[EP13] result layout: rank=%u base=(%.1f,%.1f) cached=(%.1f,%.1f) scale=%.2f/%.2f\n",
                id, base.position.x, base.position.y, row->positionAndscale[1].position.x,
                row->positionAndscale[1].position.y, row->positionAndscale[1].scale.x,
                row->positionAndscale[1].scale.z);
        return;
    }
    if(id > VanillaCount) row->id = VanillaCount;
    EPResultLoad(row);
    row->id = id;
    for(u32 i = 0; i < 4; ++i) {
        PositionAndScale& pos = row->positionAndscale[i];
        if(id == 1) firstRow[i] = pos;
        if(id == 2) rowStep[i].position.y = pos.position.y - firstRow[i].position.y;
        if(id > VanillaCount) pos.position.y += rowStep[i].position.y * (id - VanillaCount);
        pos.position.y = firstRow[i].position.y + (pos.position.y - firstRow[i].position.y) * 11.0f / static_cast<float>(Racedata::sInstance->racesScenario.playerCount - 1);
        pos.scale.z *= 11.0f / static_cast<float>(Racedata::sInstance->racesScenario.playerCount - 1);
    }
}
kmWritePointer(0x808d3f24, LoadResult);

// 0x521 is the points message, NOT a thirteenth-place ordinal.
static void ResultPosition(LayoutUIControl* row, const char* pane, u32 bmg, const Text::Info* info) {
    if((IsActive() || SavedResultCount() > VanillaCount) && bmg >= 0x521 && bmg <= 0x514 + Capacity) {
        static wchar_t ordinals[Capacity - VanillaCount][5] = {L"13th", L"14th", L"15th", L"16th", L"17th", L"18th", L"19th", L"20th", L"21st", L"22nd", L"23rd", L"24th"};
        Text::Info text;
        text.strings[0] = ordinals[bmg - 0x521];
        row->SetTextBoxMessage(pane, UI::BMG_TEXT, &text);
    }
    else row->SetTextBoxMessage(pane, bmg, info);
}
kmCall(0x807f6028, ResultPosition);
kmCall(0x805bbc4c, ResultPosition);

static void ScoreResults() {
    if(scored) return;
    resultCount = Racedata::sInstance->racesScenario.playerCount;
    if(!resultCount || resultCount > Capacity) { resultCount = 0; return; }
    u32 used = 0;
    u32 count = 0;
    for(u32 rank = 0; rank < resultCount; ++rank) {
        const u32 id = Raceinfo::sInstance->playerIdInEachPosition[rank];
        if(id < resultCount && !(used & (1 << id))) {
            order[count++] = id;
            used |= 1 << id;
        }
    }
    // Keep every racer even if the ranking contains a duplicate ID.
    for(u32 id = 0; id < resultCount; ++id) if(!(used & (1 << id))) order[count++] = id;
    for(u32 rank = 0; rank < resultCount; ++rank) {
        const u32 id = order[rank];
        RacedataPlayer& race = RacePlayer(id);
        before[id] = race.previousScore;
        totals[id] = before[id] + PlacementPointsForCount(resultCount, rank + 1);
        RacedataPlayer& result = id < VanillaCount ? Racedata::sInstance->menusScenario.players[id] : race;
        result.previousScore = before[id];
        result.score = totals[id];
        result.finishPos = rank + 1;
    }
    for(u32 id = 0; id < resultCount; ++id) {
        const RacedataPlayer& source = id < VanillaCount ? Racedata::sInstance->menusScenario.players[id] : RacePlayer(id);
        memcpy(savedRecords[id], &source, sizeof(source));
        // Keep the race UI name before ceremony Mii rebuild; scenario records may still say Player.
        if(source.playerType==PLAYER_REAL_LOCAL && SectionMgr::sInstance
            && SectionMgr::sInstance->sectionParams) {
            const MiiGroup& group=SectionMgr::sInstance->sectionParams->playerMiis;
            if(group.mii && id<group.miiCount && group.mii[id] && group.mii[id]->info.name[0])
                memcpy(SavedResultPlayer(id).mii.info.name,group.mii[id]->info.name,
                    sizeof(SavedResultPlayer(id).mii.info.name));
        }
        SavedResultPlayer(id).prevFinishPos = source.finishPos;
        SavedResultPlayer(id).unknown_0xe0 = source.finishPos;
    }
    scored = true;
    gpResults = IsActive() && Racedata::sInstance->racesScenario.settings.gamemode == MODE_GRAND_PRIX;
    if(gpResults) {
        // Keep native local-human GP grading when replacing the unsafe count-indexed scorer.
        for(u32 id = 0; id < resultCount; ++id)
            if(RacePlayer(id).playerType == PLAYER_REAL_LOCAL)
                EPComputeGPRank(Raceinfo::sInstance->players[id]);
    }
    OS::Report("[EP13] results: %u entrants; count-based points first=%u last=%u\n",
        resultCount, PlacementPointsForCount(resultCount, 1), PlacementPointsForCount(resultCount, resultCount));
}
static void UpdateRows(Pages::GPVSLeaderboardUpdate* page) {
    if(!UsesVSScoring()) { EPResultUpdateRows(page); return; }
    ScoreResults();
    for(u32 rank = 0; rank < resultCount; ++rank) {
        const u8 id = order[rank];
        CtrlRaceResult* row = page->results[rank];
        if(resultCount > VanillaCount) ApplyResultLayout(row);
        EPResultFill(row, rank + 1, id);
        EPResultFillScore(row, before[id], 0x521);
        EPResultAnimateScore(row, totals[id] - before[id], rank == 0);
        row->FillName(id);
        OS::Report("[EP13] result row=%u id=%u score=%u+%u\n", rank + 1, id, before[id], totals[id] - before[id]);
    }
    page->func_0x6c();
}
kmWritePointer(0x808dac80, UpdateRows);
static void TotalRows(Pages::GPVSLeaderboardTotal* page) {
    if(!UsesVSScoring()) { EPResultTotalRows(page); return; }
    ScoreResults();
    // Sort by total score, using the previous race order to break ties.
    for(u32 rank = 0; rank < resultCount; ++rank) {
        const u8 id = order[rank];
        Pages::GPVSLeaderboardUpdate::Player& entry = page->sortedArray[rank];
        entry.playerId = id;
        entry.totalScore = totals[id];
        entry.lastRaceScore = totals[id] - before[id];
    }
    for(u32 i = 1; i < resultCount; ++i) {
        const Pages::GPVSLeaderboardUpdate::Player entry = page->sortedArray[i];
        u32 j = i;
        while(j && page->sortedArray[j - 1].totalScore < entry.totalScore) {
            page->sortedArray[j] = page->sortedArray[j - 1]; --j;
        }
        page->sortedArray[j] = entry;
    }
    for(u32 rank = 0; rank < resultCount; ++rank) {
        const u8 id = page->sortedArray[rank].playerId;
        CtrlRaceResult* row = page->results[rank];
        if(resultCount > VanillaCount) ApplyResultLayout(row);
        EPResultFill(row, rank + 1, id);
        EPResultFillScore(row, totals[id], 0x521);
        row->ResetTextBoxMessage("get_point");
        if(id < VanillaCount) Racedata::sInstance->menusScenario.players[id].prevFinishPos = rank + 1;
        else RacePlayer(id).prevFinishPos = rank + 1;
        SavedResultPlayer(id).prevFinishPos = rank + 1;
        SavedResultPlayer(id).unknown_0xe0 = rank + 1;
    }
    OS::Report("[EP13] total results: all %u rows filled\n", resultCount);
}
kmWritePointer(0x808dac0c, TotalRows);
} }
