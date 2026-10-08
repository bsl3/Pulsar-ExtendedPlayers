#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceRankNum.hpp>
#include <core/rvl/OS/OS.hpp>
#include <ExtendedPlayers/Presentation/ExtendedPositionTexture.hpp>

extern "C" {
void EPPositionDraw(LayoutUIControl*, u32);
void EPPositionAnimate(MainLayout*);
}

namespace Pulsar { namespace ExtendedPlayers {
static CtrlRaceRankNum* drawingRank;

static void DrawRank(CtrlRaceRankNum* control, u32 z) {
    CtrlRaceRankNum* previous = drawingRank;
    drawingRank = IsActive() ? control : static_cast<CtrlRaceRankNum*>(nullptr);
    EPPositionDraw(control, z);
    drawingRank = previous;
}
kmWritePointer(0x808d3eac, DrawRank);

static void AnimatePosition(MainLayout* layout) {
    EPPositionAnimate(layout);
    CtrlRaceRankNum* control = drawingRank;
    if(!control || layout != &control->layout) return;
    // Use prevPosition, which native code changes at the rank animation boundary, not raw Raceinfo rank.
    const u32 rank = control->prevPosition;
    if(rank <= VanillaCount || rank > Capacity) return;
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const bool multi = scenario.screenCount != 1 || (scenario.settings.modeFlags & 2);
    char name[64];
    snprintf(name, sizeof(name), multi ? "tt_multi_position_no_st_64x64_%02u.tpl"
        : "tt_position_no_st_64x64_%02u.tpl", rank);
    TPLPalettePtr texture = static_cast<TPLPalettePtr>(
        layout->resources->multiArcResourceAccessor.GetResource(lyt::res::RESOURCETYPE_TEXTURE, name));
    if(!texture && Config::PositionIconFallback >= 13 && Config::PositionIconFallback <= Capacity) {
        snprintf(name, sizeof(name), multi ? "tt_multi_position_no_st_64x64_%02u.tpl"
            : "tt_position_no_st_64x64_%02u.tpl", Config::PositionIconFallback);
        texture = static_cast<TPLPalettePtr>(
            layout->resources->multiArcResourceAccessor.GetResource(lyt::res::RESOURCETYPE_TEXTURE, name));
    }
    static u32 reported[2][4];
    const u32 slot = control->hudSlotId;
    if(slot >= 4) return;
    const u32 bit = 1u << rank;
    if(!(reported[multi][slot] & bit)) {
        OS::Report("[EP13] position HUD: P%u rank=%u family=%s texture=%p (%s)\n",
            slot + 1, rank, multi ? "multi" : "single", texture, name);
        reported[multi][slot] |= bit;
    }
    const GX::TexObj* numeric = texture ? static_cast<const GX::TexObj*>(nullptr) : NumericPositionTexture(rank);
    if(!texture && !numeric) return;
    // Rebind after Animate and before draw; RLTP otherwise restores rank 12 on all three materials.
    static const char* panes[] = {"position_sha", "position", "position_l_00"};
    for(u32 i = 0; i < 3; ++i) {
        lyt::Pane* pane = layout->GetPaneByName(panes[i]);
        if(pane && pane->GetMaterial()) {
            lyt::TexMap* map = pane->GetMaterial()->GetTexMapAry();
            if(texture) map->ReplaceImage(texture);
            else map->Set(*numeric);
        }
    }
}
kmCall(0x8063dc4c, AnimatePosition);
} }
