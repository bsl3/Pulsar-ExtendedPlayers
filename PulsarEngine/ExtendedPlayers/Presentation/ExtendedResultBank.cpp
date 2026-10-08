#include <ExtendedPlayers/Presentation/ExtendedResultBank.hpp>
#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <ExtendedPlayers/Core/MemoryAccounting.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/GPVSLeaderboardTotal.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <core/egg/mem/ExpHeap.hpp>

extern "C" {
void EPResultUpdateInit(Pages::GPVSLeaderboardUpdate*);
void EPResultTotalInit(Pages::GPVSLeaderboardTotal*);
void EPLeaderboardActivate(Pages::Leaderboard*);
}
namespace Pulsar { namespace ExtendedPlayers {
namespace {
struct InitialAnimation {
    u32 animation;
    float frame, unknown;
    bool active;
};
struct InitialRow {
    PositionAndScale positions[4];
    u8 nativeTail[sizeof(CtrlRaceResult)-0x174];
    InitialAnimation* animations;
    bool hidden;
};
// One Section owns the bank. Update deactivates before Total activates.
// Share the row layouts and animations. Each page keeps its own group, scores and sortedArray.
struct ResultBank {
    Pages::GPVSLeaderboardUpdate* update;
    Pages::GPVSLeaderboardTotal* total;
    CtrlRaceResult* rows[Capacity];
    InitialRow initial[Capacity];
    u32 groupCount;
    ResultBank(Pages::GPVSLeaderboardUpdate* page):update(page),total(nullptr),groupCount(0) {
        memset(rows,0,sizeof(rows));memset(initial,0,sizeof(initial));
    }
    void Capture() {
        groupCount=update->results[0]->animator.animationCount;
        for(u32 i=0;i<Capacity;++i) {
            CtrlRaceResult* row=rows[i]=update->results[i];
            InitialRow& state=initial[i];
            memcpy(state.positions,row->positionAndscale,sizeof(state.positions));
            memcpy(state.nativeTail,reinterpret_cast<u8*>(row)+0x174,sizeof(state.nativeTail));
            state.hidden=row->isHidden;
            state.animations=new InitialAnimation[groupCount];
            for(u32 g=0;g<groupCount;++g) {
                const AnimationGroup& group=row->animator.animationGroups[g];
                InitialAnimation& a=state.animations[g];
                a.animation=group.curAnimation;a.frame=group.curFrame;
                a.unknown=group.unknown_0x40;a.active=group.isActive;
            }
        }
    }
    void Prepare(Pages::GPVSLeaderboardUpdate* page) {
        for(u32 i=0;i<Capacity;++i) {
            CtrlRaceResult* row=rows[i];const InitialRow& state=initial[i];
            // Move the rows to this page before filling them; reset animations so old points do not appear on Total.
            row->SetParentControlGroup(&page->controlGroup,0);
            memcpy(row->positionAndscale,state.positions,sizeof(state.positions));
            memcpy(reinterpret_cast<u8*>(row)+0x174,state.nativeTail,sizeof(state.nativeTail));
            row->isHidden=state.hidden;
            for(u32 g=0;g<groupCount;++g) {
                AnimationGroup& group=row->animator.animationGroups[g];
                const InitialAnimation& a=state.animations[g];
                group.PlayAnimationAtFrame(a.animation,a.frame);
                group.unknown_0x40=a.unknown;group.isActive=a.active;
            }
        }
    }
    ~ResultBank() {
        // ControlGroup only frees its pointer array. Delete each shared row once, before its allocator and resources go away.
        for(u32 i=0;i<Capacity;++i) {
            rows[i]->parentGroup=nullptr;
            delete rows[i];delete[] initial[i].animations;
        }
    }
};
static ResultBank* bank;
static bool building;
static u32 rowLoadBytes, measuredRows;
static bool Enabled() {
    return IsActive() && Racedata::sInstance->racesScenario.playerCount==Capacity;
}
static u32 FreeRace() {
    const GameScene* scene=GameScene::GetCurrent();
    return scene->structsMem1->getTotalFreeSize()+scene->structsMem2->getTotalFreeSize();
}
static void UpdateInit(Pages::GPVSLeaderboardUpdate* page) {
    if(!Enabled() || bank) { EPResultUpdateInit(page);return; }
    const u32 before=FreeRace();
    // The retail Update constructor leaves its Total-only pointer uninitialized.
    page->sortedArray=nullptr;
    building=true;rowLoadBytes=measuredRows=0;
    EPResultUpdateInit(page);
    building=false;
    bank=new ResultBank(page);bank->Capture();
    OS::Report("[EPResultBank] built rows=%u groups=%u raceCharge=%u snapshots=%u repeatLoadBytes=%u measuredRows=%u; Total reuses native rows\n",
        Capacity,bank->groupCount,before-FreeRace(),sizeof(ResultBank)+Capacity*bank->groupCount*sizeof(InitialAnimation),rowLoadBytes,measuredRows);
    MemoryAccounting::Report("result bank built",false);
}
static void TotalInit(Pages::GPVSLeaderboardTotal* page) {
    if(!Enabled() || !bank || bank->total) { EPResultTotalInit(page);return; }
    const u32 before=FreeRace();
    page->sortedArray=new Pages::GPVSLeaderboardUpdate::Player[Capacity];
    page->InitControlGroup(Capacity);
    page->results=new CtrlRaceResult*[Capacity];
    bank->total=page;
    for(u32 i=0;i<Capacity;++i) {
        page->results[i]=bank->rows[i];page->AddControl(i,*page->results[i],0);
    }
    CopyResultBankMetrics();
    OS::Report("[EPResultBank] Total reused rows=%u newRaceCharge=%u nativeLoads=0\n",Capacity,before-FreeRace());
    MemoryAccounting::Report("result bank reused",false);
}
static void Activate(Pages::GPVSLeaderboardUpdate* page) {
    if(bank && (page==bank->update || page==bank->total)) {
        bank->Prepare(page);
        OS::Report("[EPResultBank] activate page=%u rows=%u restored native animations; allocations=0\n",page->pageId,Capacity);
    }
    EPLeaderboardActivate(page);
}
static void Dispose(Pages::GPVSLeaderboardUpdate* page) {
    if(!bank || (page!=bank->update && page!=bank->total))return;
    // Clear the other page's row pointers too; either page may be destroyed first or fail during initialization.
    page->controlGroup.controlCount=0;
    for(u32 i=0;i<Capacity;++i) {
        page->controlGroup.controlArray[i]=nullptr;
        page->controlGroup.zIdxOrderedArray[i]=nullptr;
    }
    delete[] page->results;page->results=nullptr;
    delete[] page->sortedArray;page->sortedArray=nullptr;
    if(page==bank->update)bank->update=nullptr;else bank->total=nullptr;
    if(!bank->update && !bank->total) {
        ResultBank* retired=bank;bank=nullptr;delete retired;
        OS::Report("[EPResultBank] disposed rows=%u once; bank cleared\n",Capacity);
    }
}
}
void RecordResultBankRowCost(unsigned rank,unsigned bytes) {
    // Exclude the first Load's shared accessor/picture cost from per-row measurements.
    // Scene-only totals are a lower bound when system memory backs native row objects.
    if(building && rank>1) { rowLoadBytes+=bytes;++measuredRows; }
}
kmWritePointer(0x808dac40,UpdateInit);
kmWritePointer(0x808dabcc,TotalInit);
kmWritePointer(0x808dac48,Activate);
kmWritePointer(0x808dabd4,Activate);
kmWritePointer(0x808dac44,Dispose);
kmWritePointer(0x808dabd0,Dispose);
} }
