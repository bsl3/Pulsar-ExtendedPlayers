// These hooks save the caller's registers before looking up extra players.
#ifndef PUL_CAPACITY_CONTEXT
#define PUL_CAPACITY_CONTEXT
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


#endif
