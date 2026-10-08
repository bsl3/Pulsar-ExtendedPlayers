#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <ExtendedPlayers/Core/CapacityContext.hpp>
#include <ExtendedPlayers/Presentation/ExtendedAwardsNative.hpp>
#include <core/egg/mem/Heap.hpp>
#include <UI/UI.hpp>
extern "C" void EPResultResetPosition(UIControl*);
namespace Pulsar { namespace ExtendedPlayers {
class ExtendedAwardsPage : public Pages::AwardResults {
public:
    Pages::AwardDemoResultItem extra[Capacity - VanillaCount];
    void OnInit() override {
        // Keep native controls/transitions; the allocation hook adds row slots.
        Pages::AwardResults::OnInit();
        const u32 count = SavedResultCount();
        const u32 rows = Config::ResultRowsPerColumn(count);
        for(u32 id = VanillaCount; id < count; ++id) {
            AddControl(id + 3, extra[id - VanillaCount], 0);
            extra[id - VanillaCount].Load(id % rows, false, false);
        }
        FillRows();
    }
    void FillRows() {
        const u32 count=SavedResultCount();
        for(u32 rank = 0; rank < count; ++rank) {
            for(u32 id = 0; id < count; ++id)
                if(SavedResultPlayer(id).prevFinishPos == rank + 1) {
                    Row(rank).SetValues(id, false, false);
                    RacedataPlayer& player=SavedResultPlayer(id);
                    if(player.playerType==PLAYER_REAL_LOCAL && player.mii.info.name[0]) {
                        Text::Info text; text.strings[0]=player.mii.info.name;
                        Row(rank).SetTextBoxMessage("mii_name",UI::BMG_TEXT,&text);
                    }
                    break;
                }
            LayoutAwardRow(&Row(rank),rank);
        }
    }
    void ApplyLayout() {
        for(u32 rank=0;rank<SavedResultCount();++rank)LayoutAwardRow(&Row(rank),rank,false);
        LayoutHeader();
    }
    void LayoutHeader() {
        LayoutUIControl* controls[]={&awardTypeWin,&awardRankWin,&congratulations};
        const float x[]={-200.0f,200.0f,0.0f};
        for(u32 i=0;i<3;++i) {
            PositionAndScale& p=controls[i]->positionAndscale[0];
            p.position.x=x[i];p.position.y=224.0f;
            p.scale.x=p.scale.z=i==2 ? 0.28f : 0.40f;
            EPResultResetPosition(controls[i]);
        }
    }
    Pages::AwardDemoResultItem& Row(u32 rank) {
        return rank < VanillaCount ? demoResultItems[rank] : extra[rank - VanillaCount];
    }
};
// Native AnimateControls rewrites row transforms. Apply the extended layout afterward.
extern "C" void EPAwardNativeAnimateControls(Page*);
static void AnimateAwardControls(Page* page) {
    EPAwardNativeAnimateControls(page);
    if(page->pageId==PAGE_AWARD_RESULTS && SavedResultCount()>VanillaCount)
        static_cast<ExtendedAwardsPage*>(page)->ApplyLayout();
}
kmCall(0x80602320,AnimateAwardControls);
static_assert(sizeof(ExtendedAwardsPage) == 0x28e0, "Awards overflow control ownership");
static void* AllocateAwards(u32 originalSize) {
    return EGG::Heap::alloc(SavedResultCount() > VanillaCount ? sizeof(ExtendedAwardsPage) : originalSize, 4);
}
static Pages::AwardResults* ConstructAwards(void* storage) {
    if(SavedResultCount() > VanillaCount) return new(storage) ExtendedAwardsPage;
    return new(storage) Pages::AwardResults;
}
static void InitAwardControls(Page* page, u32 originalCount) {
    page->InitControlGroup(SavedResultCount() > VanillaCount ? SavedResultCount() + 3 : originalCount);
}
// Native factory allocation + constructor and OnInit's control-group call.
// Original PAL targets: operator new, AwardResults ctor, Page::InitControlGroup.
kmCall(0x80623900, AllocateAwards);
kmCall(0x8062390c, ConstructAwards);
kmCall(0x805bc28c, InitAwardControls);
extern "C" u32 EPAwardRecordBase(u32 id) {
    // SetValues adds menusScenario.players (+0x1808) to r31. Redirect that player view only.
    if(SavedResultCount() > VanillaCount && id < SavedResultCount())
        return reinterpret_cast<u32>(&SavedResultPlayer(id)) - 0x1808;
    return reinterpret_cast<u32>(Racedata::sInstance) + id * sizeof(RacedataPlayer);
}
asmFunc EPAwardRecordAdapter() {
    EP_SAVE_CONTEXT
    ASM(lwz r3, 0x48(r1); bl EPAwardRecordBase; stw r3, 0xb4(r1);)
    EP_RESTORE_CONTEXT
}
kmCall(0x805bbbfc, EPAwardRecordAdapter);
extern "C" u32 EPAwardFinalScore(u32 base) {
    // Only extended snapshot rows use final totals; keep native 12 unchanged.
    const u32 offset = SavedResultCount() > VanillaCount ? 0x18e2 : 0x18e0;
    return *reinterpret_cast<const u16*>(base + offset);
}
asmFunc EPAwardScoreAdapter() {
    EP_SAVE_CONTEXT
    ASM(lwz r3, 0xb4(r1); bl EPAwardFinalScore; stw r3, 0xa0(r1);)
    EP_RESTORE_CONTEXT
}
kmCall(0x805bbd90, EPAwardScoreAdapter);
} }

