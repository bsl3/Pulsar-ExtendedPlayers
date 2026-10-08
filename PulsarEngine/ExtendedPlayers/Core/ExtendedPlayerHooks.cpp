// Save GPRs, CR/LR/CTR/XER, f0-f13 and FPSCR. Helpers return the displaced result.
// The surrounding function saved LR except for the non-linking leaf detour below.
#include <kamek.hpp>
extern "C" u32 EPBalloonOffset(void*, u32);
extern "C" u32 EPCapCount(u32);
extern "C" u32 EPDriverOffset(u32);
extern "C" u32 EPInputOffset(u32);
extern "C" u32 EPPrimaryOffset(u32);
extern "C" u32 EPScenarioOffset(u32);
extern "C" u32 EPSecondaryOffset(u32);

#define EP_SAVE_CONTEXT ASM( \
    nofralloc; \
    stwu r1, -0x150(r1); \
    stw r0, 0x38(r1); \
    stmw r2, 0x40(r1); \
    mfcr r0; \
    stw r0, 0xb8(r1); \
    mflr r0; \
    stw r0, 0xbc(r1); \
    mfctr r0; \
    stw r0, 0xc0(r1); \
    mfxer r0; \
    stw r0, 0xc4(r1); \
    stfd f0, 0xd0(r1); \
    stfd f1, 0xd8(r1); \
    stfd f2, 0xe0(r1); \
    stfd f3, 0xe8(r1); \
    stfd f4, 0xf0(r1); \
    stfd f5, 0xf8(r1); \
    stfd f6, 0x100(r1); \
    stfd f7, 0x108(r1); \
    stfd f8, 0x110(r1); \
    stfd f9, 0x118(r1); \
    stfd f10, 0x120(r1); \
    stfd f11, 0x128(r1); \
    stfd f12, 0x130(r1); \
    stfd f13, 0x138(r1); \
    mffs f0; \
    stfd f0, 0x140(r1); \
)

#define EP_RESTORE_CONTEXT_BODY ASM( \
    lfd f0, 0x140(r1); \
    mtfsf 255, f0; \
    lfd f0, 0xd0(r1); \
    lfd f1, 0xd8(r1); \
    lfd f2, 0xe0(r1); \
    lfd f3, 0xe8(r1); \
    lfd f4, 0xf0(r1); \
    lfd f5, 0xf8(r1); \
    lfd f6, 0x100(r1); \
    lfd f7, 0x108(r1); \
    lfd f8, 0x110(r1); \
    lfd f9, 0x118(r1); \
    lfd f10, 0x120(r1); \
    lfd f11, 0x128(r1); \
    lfd f12, 0x130(r1); \
    lfd f13, 0x138(r1); \
    lwz r0, 0xc4(r1); \
    mtxer r0; \
    lwz r0, 0xc0(r1); \
    mtctr r0; \
    lwz r0, 0xbc(r1); \
    mtlr r0; \
    lwz r0, 0xb8(r1); \
    mtcrf 255, r0; \
    lmw r2, 0x40(r1); \
    lwz r0, 0x38(r1); \
    addi r1, r1, 0x150; \
)
#define EP_RESTORE_CONTEXT EP_RESTORE_CONTEXT_BODY ASM(blr;)

asmFunc EPScenarioOffsetR4ToR4() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x48(r1);
        bl EPScenarioOffset;
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPScenarioOffsetR0ToR0() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x38(r1);
        bl EPScenarioOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPScenarioOffsetR0ToR25() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x38(r1);
        bl EPScenarioOffset;
        stw r3, 0x9c(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPScenarioOffsetR3ToR4() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
        bl EPScenarioOffset;
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPScenarioOffsetR5ToR0() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x4c(r1);
        bl EPScenarioOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPScenarioOffsetR4ToR0() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x48(r1);
        bl EPScenarioOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPScenarioOffsetR31ToR0() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xb4(r1);
        bl EPScenarioOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPScenarioOffsetR28ToR0() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xa8(r1);
        bl EPScenarioOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPPrimaryOffsetR4ToR0() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x48(r1);
        bl EPPrimaryOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPSecondaryOffsetR4ToR0() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x48(r1);
        bl EPSecondaryOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPInputOffsetR5ToR0() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x4c(r1);
        bl EPInputOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPDriverOffsetR28ToR0() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xa8(r1);
        bl EPDriverOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPCapCountR3ToR4() {
    EP_SAVE_CONTEXT
    ASM(
        lbz r3, 0x24(r3);
        bl EPCapCount;
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT
}

asmFunc EPCapCountR31ToR5() {
    EP_SAVE_CONTEXT
    ASM(
        lbz r3, 0x10(r31);
        bl EPCapCount;
        stw r3, 0x4c(r1);
    )
    EP_RESTORE_CONTEXT
}


kmCall(0x80553fbc, EPScenarioOffsetR4ToR4);
kmCall(0x80553fdc, EPScenarioOffsetR0ToR0);
kmCall(0x80554018, EPScenarioOffsetR4ToR4);
kmCall(0x805540c0, EPScenarioOffsetR0ToR25);
kmCall(0x8058fd5c, EPScenarioOffsetR3ToR4);
kmCall(0x80592fd8, EPScenarioOffsetR0ToR0);
kmCall(0x8058e25c, EPScenarioOffsetR0ToR0);
kmCall(0x8059443c, EPScenarioOffsetR0ToR0);
kmCall(0x80532924, EPScenarioOffsetR0ToR0);
kmCall(0x80534118, EPScenarioOffsetR5ToR0);
// Raceinfo progress, lap/finish classification and finish-controller transfer.
kmCall(0x8053532c, EPScenarioOffsetR4ToR4);
kmCall(0x80535434, EPScenarioOffsetR0ToR0);
kmCall(0x80534bec, EPScenarioOffsetR0ToR0);
kmCall(0x805348d8, EPScenarioOffsetR5ToR0);
// Native finish code indexes twelve embedded scenario records. Use the sidecar view
// for extra racers; ID 12 otherwise aliases course settings and controller/type fields.
kmCall(0x805361a4, EPScenarioOffsetR5ToR0);
kmCall(0x80533c78, EPScenarioOffsetR4ToR0);
kmCall(0x805414b4, EPScenarioOffsetR4ToR0);
kmCall(0x807bd680, EPScenarioOffsetR31ToR0);
kmCall(0x80726264, EPScenarioOffsetR0ToR0);
kmCall(0x807262a8, EPScenarioOffsetR0ToR0);
kmCall(0x807262f0, EPScenarioOffsetR0ToR0);
kmCall(0x80726354, EPScenarioOffsetR0ToR0);
// Native CPU group selection reads team/previous-score fields by racer ID.
kmCall(0x80742254, EPScenarioOffsetR0ToR0);
kmCall(0x80742320, EPScenarioOffsetR0ToR0);
kmCall(0x80742350, EPScenarioOffsetR0ToR0);
kmCall(0x807423ec, EPScenarioOffsetR0ToR0);
kmCall(0x80742480, EPScenarioOffsetR0ToR0);
kmCall(0x80742534, EPScenarioOffsetR0ToR0);
kmCall(0x80797724, EPScenarioOffsetR28ToR0);
kmCall(0x80540e48, EPPrimaryOffsetR4ToR0);
kmCall(0x80540f9c, EPSecondaryOffsetR4ToR0);
kmCall(0x8054132c, EPPrimaryOffsetR4ToR0);
kmCall(0x805413d4, EPPrimaryOffsetR4ToR0);
kmCall(0x80541444, EPSecondaryOffsetR4ToR0);
kmCall(0x8053415c, EPInputOffsetR5ToR0);
kmCall(0x805348f4, EPInputOffsetR5ToR0);

kmCall(0x8078cbd8, EPCapCountR3ToR4);
kmCall(0x80799410, EPCapCountR31ToR5);

kmCall(0x8079775c, EPDriverOffsetR28ToR0);



asmFunc EPBalloonWrite() {
    EP_SAVE_CONTEXT
    ASM(
        mr r4, r3;
        mr r3, r30;
        bl EPBalloonOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}
asmFunc EPBalloonCompare() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r4, 0x38(r1);
        mr r3, r30;
        bl EPBalloonOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}
asmFunc EPBalloonName() {
    EP_SAVE_CONTEXT
    ASM(
        mr r4, r27;
        lwz r3, 0x17c(r30);
        bl EPBalloonOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}
asmFunc EPBalloonItem() {
    EP_SAVE_CONTEXT
    ASM(
        mr r4, r30;
        lwz r3, 0x188(r29);
        bl EPBalloonOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807f174c, EPBalloonWrite);
kmCall(0x807f1ad4, EPBalloonCompare);
kmCall(0x807f111c, EPBalloonName);
kmCall(0x807f3a34, EPBalloonItem);
// Minimap player type/character/team, including inlined ownership checks.
kmCall(0x807eb2a0, EPScenarioOffsetR4ToR0);
kmCall(0x807eb418, EPScenarioOffsetR0ToR0);
kmCall(0x807eb548, EPScenarioOffsetR4ToR0);
kmCall(0x807eb75c, EPScenarioOffsetR0ToR0);
kmCall(0x807eb8d4, EPScenarioOffsetR0ToR0);
kmCall(0x807eb958, EPScenarioOffsetR0ToR0);
kmCall(0x807f18f4, EPScenarioOffsetR0ToR0);
kmCall(0x807f19e0, EPScenarioOffsetR0ToR0);

asmFunc EPScenarioOffsetR30ToR31() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r30;
        bl EPScenarioOffset;
        stw r3, 0xb4(r1);
    )
    EP_RESTORE_CONTEXT
}
asmFunc EPScenarioOffsetR30ToR0() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r30;
        bl EPScenarioOffset;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807f6038, EPScenarioOffsetR30ToR31);
kmCall(0x807f6344, EPScenarioOffsetR30ToR0);
kmCall(0x807f6424, EPScenarioOffsetR30ToR0);
kmCall(0x807f5324, EPScenarioOffsetR31ToR0);

asmFunc EPScenarioOffsetR3ToR29() {
    EP_SAVE_CONTEXT
    ASM(
        bl EPScenarioOffset;
        stw r3, 0xac(r1);
    )
    EP_RESTORE_CONTEXT
}
// Name balloons: type, team, local colour, character and CPU name.
kmCall(0x807f008c, EPScenarioOffsetR4ToR0);
kmCall(0x807f0134, EPScenarioOffsetR30ToR0);
kmCall(0x807f0344, EPScenarioOffsetR30ToR0);
kmCall(0x807f0464, EPScenarioOffsetR30ToR31);
// Lightning source/target team filtering.
kmCall(0x807b7c34, EPScenarioOffsetR3ToR29);
kmCall(0x807b7c8c, EPScenarioOffsetR0ToR0);

extern "C" void EPRouteResume();
extern "C" void* EPRouteAt(void*, u32);
extern "C" u32 EPThunderCount();
// 8073C394 has no saved LR. Enter/resume with b and preserve LR across the helper.
asmFunc EPRouteRead() {
    EP_SAVE_CONTEXT
    ASM(
        mr r4, r31;
        bl EPRouteAt;
        stw r3, 0xb0(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPRouteResume;)
}
asmFunc EPThunderLiveCount() {
    EP_SAVE_CONTEXT
    ASM(
        bl EPThunderCount;
        stw r3, 0xa0(r1);
    )
    EP_RESTORE_CONTEXT
}
kmBranch(0x8073c408, EPRouteRead);
kmCall(0x807b7c50, EPThunderLiveCount);
extern "C" void EPCloudDriverTarget(u32, void*, s32, s32);
extern "C" void EPCloudDriverResume();
// Replace the whole target block: ID 12 also overruns its timer and targeting arrays.
asmFunc EPCloudDriver() {
    EP_SAVE_CONTEXT
    ASM(
        mr r4, r27;
        mr r5, r29;
        mr r6, r30;
        bl EPCloudDriverTarget;
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPCloudDriverResume;)
}
kmBranch(0x8078dd14, EPCloudDriver);
extern "C" void* EPItemDriverLookup(void*, const void*);
extern "C" void EPItemDriver807A0B00Resume();
asmFunc EPItemDriver807A0B00() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x4c(r1);
lwz r4, 0x44(r1);
        bl EPItemDriverLookup;
        stw r3, 0x4c(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807A0B00Resume;)
}
kmBranch(0x807a0b00, EPItemDriver807A0B00);
extern "C" void EPItemDriver807A2600Resume();
asmFunc EPItemDriver807A2600() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
lwz r4, 0xa8(r1);
        bl EPItemDriverLookup;
        stw r3, 0xb0(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807A2600Resume;)
}
kmBranch(0x807a2600, EPItemDriver807A2600);
extern "C" void EPItemDriver807A57F4Resume();
asmFunc EPItemDriver807A57F4() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x48(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807A57F4Resume;)
}
kmBranch(0x807a57f4, EPItemDriver807A57F4);
extern "C" void EPItemDriver807A9484Resume();
asmFunc EPItemDriver807A9484() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807A9484Resume;)
}
kmBranch(0x807a9484, EPItemDriver807A9484);
extern "C" void EPItemDriver807AF71CResume();
asmFunc EPItemDriver807AF71C() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807AF71CResume;)
}
kmBranch(0x807af71c, EPItemDriver807AF71C);
extern "C" void EPItemDriver807AF730Resume();
asmFunc EPItemDriver807AF730() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807AF730Resume;)
}
kmBranch(0x807af730, EPItemDriver807AF730);
extern "C" void EPItemDriver807AFD6CResume();
asmFunc EPItemDriver807AFD6C() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807AFD6CResume;)
}
kmBranch(0x807afd6c, EPItemDriver807AFD6C);
extern "C" void EPItemDriver807AFD80Resume();
asmFunc EPItemDriver807AFD80() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807AFD80Resume;)
}
kmBranch(0x807afd80, EPItemDriver807AFD80);
extern "C" void EPItemDriver807AFF10Resume();
asmFunc EPItemDriver807AFF10() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807AFF10Resume;)
}
kmBranch(0x807aff10, EPItemDriver807AFF10);
extern "C" void EPItemDriver807AFF24Resume();
asmFunc EPItemDriver807AFF24() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807AFF24Resume;)
}
kmBranch(0x807aff24, EPItemDriver807AFF24);
extern "C" void EPItemDriver807B2450Resume();
asmFunc EPItemDriver807B2450() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807B2450Resume;)
}
kmBranch(0x807b2450, EPItemDriver807B2450);
extern "C" void EPItemDriver807B68BCResume();
asmFunc EPItemDriver807B68BC() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x4c(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x4c(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807B68BCResume;)
}
kmBranch(0x807b68bc, EPItemDriver807B68BC);
extern "C" void EPItemDriver807B7D58Resume();
asmFunc EPItemDriver807B7D58() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver807B7D58Resume;)
}
kmBranch(0x807b7d58, EPItemDriver807B7D58);
extern "C" void EPCopySphereTarget(u32, void*, s32, s32);
extern "C" void EPCopySphereResume();
asmFunc EPCopySphere() {
    EP_SAVE_CONTEXT
    ASM(
        mr r4, r25;
        mr r5, r31;
        mr r6, r27;
        bl EPCopySphereTarget;
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPCopySphereResume;)
}
kmBranch(0x8078dbb0, EPCopySphere);
extern "C" u32 EPItemProbabilityRow(u32, const u32*, const void*);
extern "C" u32 EPItemRouletteRow(u32);
extern "C" void EPItemProbabilityResume();
extern "C" void EPItemRouletteResume();
asmFunc EPItemProbabilityIndex() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r21;
        mr r4, r5;
        mr r5, r31;
        bl EPItemProbabilityRow;
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemProbabilityResume;)
}
asmFunc EPItemRouletteIndex() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r5;
        bl EPItemRouletteRow;
        stw r3, 0x38(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemRouletteResume;)
}
kmBranch(0x807bb618, EPItemProbabilityIndex);
kmBranch(0x807bb8f0, EPItemRouletteIndex);
// Kart-side item collisions: victim and attacker model lookups.
extern "C" void EPItemDriver80572804Resume();
asmFunc EPItemDriver80572804() {
    EP_SAVE_CONTEXT
    ASM(
        li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver80572804Resume;)
}
kmBranch(0x80572804, EPItemDriver80572804);
extern "C" void EPItemDriver80572ABCResume();
asmFunc EPItemDriver80572ABC() {
    EP_SAVE_CONTEXT
    ASM(
        li r4, 0;
        bl EPItemDriverLookup;
        stw r3, 0x7c(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemDriver80572ABCResume;)
}
kmBranch(0x80572abc, EPItemDriver80572ABC);
extern "C" u32 EPLightningEligibilityTimer(const void*, const void*);
asmFunc EPLightningEligibility() {
    EP_SAVE_CONTEXT
    ASM(
        mr r3, r31;
        mr r4, r18;
        bl EPLightningEligibilityTimer;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT
}
kmCall(0x807bb704, EPLightningEligibility);


// CPU roster accesses: non-linking detours also preserve unsaved leaf LR.
extern "C" void** EPAICPUAddress(void*, u32);
extern "C" u32 EPAILastGroupCount(u32);
extern "C" void EPOverflow807416A0Resume();
asmFunc EPOverflow807416A0() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xa8(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow807416A0Resume;)
}
kmBranch(0x807416a0, EPOverflow807416A0);
extern "C" void EPOverflow80741714Resume();
asmFunc EPOverflow80741714() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xb4(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80741714Resume;)
}
kmBranch(0x80741714, EPOverflow80741714);
extern "C" void EPOverflow80741738Resume();
asmFunc EPOverflow80741738() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x4c(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80741738Resume;)
}
kmBranch(0x80741738, EPOverflow80741738);
extern "C" void EPOverflow8074182CResume();
asmFunc EPOverflow8074182C() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xb4(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0xa8(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow8074182CResume;)
}
kmBranch(0x8074182c, EPOverflow8074182C);
extern "C" void EPOverflow80741880Resume();
asmFunc EPOverflow80741880() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80741880Resume;)
}
kmBranch(0x80741880, EPOverflow80741880);
extern "C" void EPOverflow80741890Resume();
asmFunc EPOverflow80741890() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80741890Resume;)
}
kmBranch(0x80741890, EPOverflow80741890);
extern "C" void EPOverflow80741908Resume();
asmFunc EPOverflow80741908() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xb4(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80741908Resume;)
}
kmBranch(0x80741908, EPOverflow80741908);
extern "C" void EPOverflow8074223CResume();
asmFunc EPOverflow8074223C() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xac(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x9c(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow8074223CResume;)
}
kmBranch(0x8074223c, EPOverflow8074223C);
extern "C" void EPOverflow80742308Resume();
asmFunc EPOverflow80742308() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xb4(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0xa0(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80742308Resume;)
}
kmBranch(0x80742308, EPOverflow80742308);
extern "C" void EPOverflow807423D4Resume();
asmFunc EPOverflow807423D4() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x94(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0xa4(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow807423D4Resume;)
}
kmBranch(0x807423d4, EPOverflow807423D4);
extern "C" void EPOverflow80742468Resume();
asmFunc EPOverflow80742468() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x9c(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x90(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80742468Resume;)
}
kmBranch(0x80742468, EPOverflow80742468);
extern "C" void EPOverflow8074251CResume();
asmFunc EPOverflow8074251C() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xb0(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0xa0(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow8074251CResume;)
}
kmBranch(0x8074251c, EPOverflow8074251C);
extern "C" void EPOverflow807427F8Resume();
asmFunc EPOverflow807427F8() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x4c(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow807427F8Resume;)
}
kmBranch(0x807427f8, EPOverflow807427F8);
extern "C" void EPOverflow80742820Resume();
asmFunc EPOverflow80742820() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x48(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x4c(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80742820Resume;)
}
kmBranch(0x80742820, EPOverflow80742820);
extern "C" void EPOverflow80742868Resume();
asmFunc EPOverflow80742868() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x4c(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x58(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80742868Resume;)
}
kmBranch(0x80742868, EPOverflow80742868);
extern "C" void EPOverflow807428D8Resume();
asmFunc EPOverflow807428D8() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xb4(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow807428D8Resume;)
}
kmBranch(0x807428d8, EPOverflow807428D8);
extern "C" void EPOverflow80742984Resume();
asmFunc EPOverflow80742984() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xb4(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x4c(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80742984Resume;)
}
kmBranch(0x80742984, EPOverflow80742984);
extern "C" void EPOverflow80742A1CResume();
asmFunc EPOverflow80742A1C() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x48(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80742A1CResume;)
}
kmBranch(0x80742a1c, EPOverflow80742A1C);
extern "C" void EPOverflow80742A98Resume();
asmFunc EPOverflow80742A98() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xb0(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80742A98Resume;)
}
kmBranch(0x80742a98, EPOverflow80742A98);
extern "C" void EPOverflow80742D08Resume();
asmFunc EPOverflow80742D08() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x54(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80742D08Resume;)
}
kmBranch(0x80742d08, EPOverflow80742D08);
extern "C" void EPOverflow80742D48Resume();
asmFunc EPOverflow80742D48() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x54(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80742D48Resume;)
}
kmBranch(0x80742d48, EPOverflow80742D48);
extern "C" void EPOverflow80742D88Resume();
asmFunc EPOverflow80742D88() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x54(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80742D88Resume;)
}
kmBranch(0x80742d88, EPOverflow80742D88);
extern "C" void EPOverflow80743080Resume();
asmFunc EPOverflow80743080() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xa0(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x98(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80743080Resume;)
}
kmBranch(0x80743080, EPOverflow80743080);
extern "C" void EPOverflow807432D4Resume();
asmFunc EPOverflow807432D4() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x48(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow807432D4Resume;)
}
kmBranch(0x807432d4, EPOverflow807432D4);
extern "C" void EPOverflow807433A8Resume();
asmFunc EPOverflow807433A8() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x48(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow807433A8Resume;)
}
kmBranch(0x807433a8, EPOverflow807433A8);
extern "C" void EPOverflow80743530Resume();
asmFunc EPOverflow80743530() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xa4(r1);
        li r4, 0xec;
        bl EPAICPUAddress;
        lwz r3, 0(r3);
        stw r3, 0xb4(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80743530Resume;)
}
kmBranch(0x80743530, EPOverflow80743530);
extern "C" void EPOverflow80741508Resume();
asmFunc EPOverflow80741508() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0xb4(r1);
        li r4, 0xe8;
        bl EPAICPUAddress;
        lwz r4, 0x44(r1);
        stw r4, 0(r3);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80741508Resume;)
}
kmBranch(0x80741508, EPOverflow80741508);
extern "C" void EPOverflow80741AA8Resume();
asmFunc EPOverflow80741AA8() {
    EP_SAVE_CONTEXT
    ASM(
        lwzx r3, r3, r6;
        bl EPAILastGroupCount;
        stw r3, 0xa0(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPOverflow80741AA8Resume;)
}
kmBranch(0x80741aa8, EPOverflow80741AA8);


// Set the creator before Player::Init constructs its individual effects.
extern "C" u32 EPPlayerEffectCreator(u32);
extern "C" void EPPlayerEffectCreatorResume();
asmFunc EPPlayerEffectCreatorSlot() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x48(r1);
        bl EPPlayerEffectCreator;
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPPlayerEffectCreatorResume;)
}
kmBranch(0x8068f064, EPPlayerEffectCreatorSlot);

// Roulette completion must not index the adjacent ObjHolder as a partner.
extern "C" void* EPItemRoulettePartner(void*);
extern "C" void EPItemRoulettePartnerResume();
asmFunc EPItemRoulettePartnerRead() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
        bl EPItemRoulettePartner;
        stw r3, 0x48(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPItemRoulettePartnerResume;)
}
kmBranch(0x80798058, EPItemRoulettePartnerRead);


extern "C" void* EPPowFlagBase(void*, u32);
extern "C" void EPPowFlag807B1E24Resume();
asmFunc EPPowFlag807B1E24() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x44(r1);
        lwz r4, 0x48(r1);
        bl EPPowFlagBase;
        stw r3, 0xac(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPPowFlag807B1E24Resume;)
}
kmBranch(0x807b1e24, EPPowFlag807B1E24);

extern "C" void EPPowFlag807B1F30Resume();
asmFunc EPPowFlag807B1F30() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x9c(r1);
        lwz r4, 0xa8(r1);
        bl EPPowFlagBase;
        stw r3, 0x44(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPPowFlag807B1F30Resume;)
}
kmBranch(0x807b1f30, EPPowFlag807B1F30);

extern "C" void EPPowFlag807B2150Resume();
asmFunc EPPowFlag807B2150() {
    EP_SAVE_CONTEXT
    ASM(
        lwz r3, 0x98(r1);
        lwz r4, 0xa4(r1);
        bl EPPowFlagBase;
        stw r3, 0x90(r1);
    )
    EP_RESTORE_CONTEXT_BODY
    ASM(b EPPowFlag807B2150Resume;)
}
kmBranch(0x807b2150, EPPowFlag807B2150);

#undef EP_SAVE_CONTEXT
#undef EP_RESTORE_CONTEXT
#undef EP_RESTORE_CONTEXT_BODY
