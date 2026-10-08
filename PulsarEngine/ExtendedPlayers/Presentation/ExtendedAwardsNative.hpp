#ifndef PUL_EXTENDED_AWARDS_NATIVE
#define PUL_EXTENDED_AWARDS_NATIVE
#include <MarioKartWii/UI/Page/Page.hpp>
// The GameSource Award header declares a raw PTMF where the binary
// has a PTMF holder. These declarations match the binary:
// holder +0x44..+0x57, cup +0x58, input +0x5c, first row +0x5fc.
namespace Pages {
class AwardDemoResultItem : public LayoutUIControl {
public:
    AwardDemoResultItem();
    ~AwardDemoResultItem() override;
    void InitSelf() override;
    const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const override;
    const char* GetClassName() const override;
    void Load(u32, bool, bool);
    void SetValues(u32, bool, bool);
};
size_assert(AwardDemoResultItem, 0x174);
class AwardResults : public Page {
public:
    AwardResults();
    ~AwardResults() override;
    void OnInit() override;
    void OnActivate() override;
    void BeforeEntranceAnimations() override;
    void BeforeExitAnimations() override;
    void AfterControlUpdate() override;
    const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const override;
    PtmfHolder_1A<AwardResults, void, u32> onClickHandler;
    void* awardCupModel;
    PageManipulatorManager input;
    LayoutUIControl awardTypeWin, awardRankWin, congratulations;
    AwardDemoResultItem demoResultItems[12];
    u8 flags[4];
};
size_assert(AwardResults, 0x1770);
static_assert(sizeof(Page) + sizeof(PtmfHolder_1A<AwardResults, void, u32>)
    + sizeof(void*) + sizeof(PageManipulatorManager) + 3 * sizeof(LayoutUIControl)
    == 0x5fc, "Native awards row ABI");
}
#endif
