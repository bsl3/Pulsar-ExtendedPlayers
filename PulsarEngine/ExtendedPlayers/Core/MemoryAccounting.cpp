#include <ExtendedPlayers/Core/MemoryAccounting.hpp>
#include <kamek.hpp>
#include <core/rvl/MEM/MEMexpHeap.hpp>
#include <core/rvl/OS/OS.hpp>
#include <MarioKartWii/Audio/GameAudioHeap.hpp>

extern "C" void EPMemoryAllocResume();
extern "C" void EPMemoryFreeResume();
extern "C" void EPMemoryResizeResume();
extern "C" Audio::GameHeap* EPMemoryGameAudioHeap;
extern "C" u32 EPMemorySoundFree(const void* frameHeap);
// Replay the original prologue; leave allocation locking, alignment, groups and failure handling to the game.
asmFunc EPMemoryAllocOriginal() { ASM(nofralloc; stwu r1,-0x20(r1); b EPMemoryAllocResume;) }
asmFunc EPMemoryFreeOriginal() { ASM(nofralloc; stwu r1,-0x20(r1); b EPMemoryFreeResume;) }
asmFunc EPMemoryResizeOriginal() { ASM(nofralloc; stwu r1,-0x30(r1); b EPMemoryResizeResume;) }

namespace Pulsar { namespace ExtendedPlayers { namespace MemoryAccounting {
struct Counter { u32 live, peak, allocated, freed, allocs, frees; };
struct Arena {
    EGG::Heap* heap; MEM::HeapHandle raw; const char* name;
    Counter groups[17]; u32 failed, retired, live, peak, incomplete;
};
static Arena arenas[8];
static u32 generation;
static bool enabled;
static bool printing;
static bool Readable(u32 p,u32 n) {
    return (p>=0x80000000 && p<0x81800000 && n<=0x81800000-p)
        || (p>=0x90000000 && p<0x94000000 && n<=0x94000000-p);
}
static Arena* Find(MEM::HeapHandle raw) {
    if(!enabled)return nullptr;
    for(u32 i=0;i<8;++i)if(arenas[i].raw==raw && !arenas[i].retired)return &arenas[i];
    return nullptr;
}
static u32 Group(const MEM::iExpHeapMBlockHead& block) {
    const u32 id=block.attribute.val&0xff;return id<16?id:16;
}
void Begin(u32 value, bool observe) {
    const int irq=OS::DisableInterrupts();
    memset(arenas,0,sizeof(arenas));generation=value;enabled=observe;
    OS::RestoreInterrupts(irq);
}
void Watch(const char* name,EGG::Heap* heap) {
    if(!enabled || !heap || !Readable(reinterpret_cast<u32>(heap),0x14))return;
    MEM::HeapHandle raw=*reinterpret_cast<MEM::HeapHandle*>(reinterpret_cast<u8*>(heap)+0x10);
    if(!Readable(reinterpret_cast<u32>(raw),0x50) || raw->magic!=0x45585048)return;
    const int irq=OS::DisableInterrupts();
    if(Find(raw)){OS::RestoreInterrupts(irq);return;}
    Arena* a=nullptr;
    for(u32 i=0;i<8;++i)if(!arenas[i].raw || arenas[i].retired){a=&arenas[i];break;}
    if(a) {
        memset(a,0,sizeof(*a));a->heap=heap;a->raw=raw;a->name=name;
        const MEM::iExpHeapHead* exp=reinterpret_cast<const MEM::iExpHeapHead*>(reinterpret_cast<u8*>(raw)+0x3c);
        const MEM::iExpHeapMBlockHead* block=exp->usedBlocks.head;
        u32 nodes=0;
        while(block && nodes++<65536) {
            const u32 b=reinterpret_cast<u32>(block),start=reinterpret_cast<u32>(raw->startAddr),end=reinterpret_cast<u32>(raw->endAddr);
            if(b<start || b>end || end-b<0x10 || block->magic!=0x5544 || block->blockSize>end-b-0x10){a->incomplete=1;break;}
            Counter& c=a->groups[Group(*block)];c.live+=block->blockSize;c.peak=c.live;
            a->live+=block->blockSize;a->peak=a->live;
            block=block->nextBlock;
        }
        if(block)a->incomplete=1;
    }
    OS::RestoreInterrupts(irq);
}
static void* Allocate(MEM::HeapHandle raw,u32 bytes,s32 alignment) {
    void* result=reinterpret_cast<void* (*)(MEM::HeapHandle,u32,s32)>(EPMemoryAllocOriginal)(raw,bytes,alignment);
    if(!enabled || printing)return result;
    const int irq=OS::DisableInterrupts();
    Arena* a=Find(raw);
    if(a) {
        if(!result)++a->failed;
        else {
            const MEM::iExpHeapMBlockHead* b=reinterpret_cast<const MEM::iExpHeapMBlockHead*>(static_cast<u8*>(result)-0x10);
            Counter& c=a->groups[Group(*b)];c.live+=b->blockSize;c.allocated+=b->blockSize;++c.allocs;
            if(c.live>c.peak)c.peak=c.live;
            a->live+=b->blockSize;if(a->live>a->peak)a->peak=a->live;
        }
    }
    const bool failure=a && !result && a->failed<=4;
    const char* name=a?a->name:"unknown";
    OS::RestoreInterrupts(irq);
    if(failure) {
        printing=true;
        OS::Report("[MemOwner] FAIL gen=%u arena=%s requested=%u align=%d free=%u largest=%u group=%u\n",
            generation,name,bytes,alignment,MEM::GetTotalFreeSizeForExpHeap(raw),MEM::GetAllocatableSizeForExpHeapEx(raw,alignment),
            reinterpret_cast<const MEM::iExpHeapHead*>(reinterpret_cast<const u8*>(raw)+0x3c)->groupID);
        printing=false;
    }
    return result;
}
static void Free(MEM::HeapHandle raw,void* pointer) {
    if(enabled && !printing && pointer) {
        const int irq=OS::DisableInterrupts();
        Arena* a=Find(raw);
        if(a) {
            const MEM::iExpHeapMBlockHead* b=reinterpret_cast<const MEM::iExpHeapMBlockHead*>(static_cast<u8*>(pointer)-0x10);
            Counter& c=a->groups[Group(*b)];
            if(c.live<b->blockSize){a->incomplete=1;c.live=0;}else c.live-=b->blockSize;
            c.freed+=b->blockSize;++c.frees;
            if(a->live<b->blockSize){a->incomplete=1;a->live=0;}else a->live-=b->blockSize;
        }
        for(u32 i=0;i<8;++i)if(arenas[i].heap==pointer)arenas[i].retired=1;
        OS::RestoreInterrupts(irq);
    }
    reinterpret_cast<void (*)(MEM::HeapHandle,void*)>(EPMemoryFreeOriginal)(raw,pointer);
}
static u32 Resize(MEM::HeapHandle raw,void* pointer,u32 bytes) {
    Arena* a=Find(raw);
    const MEM::iExpHeapMBlockHead* block=pointer?static_cast<const MEM::iExpHeapMBlockHead*>(pointer)-1:0;
    const u32 before=a&&pointer?block->blockSize:0;
    const u32 group=a&&pointer?Group(*block):0;
    const u32 result=reinterpret_cast<u32 (*)(MEM::HeapHandle,void*,u32)>(EPMemoryResizeOriginal)(raw,pointer,bytes);
    if(a && result && !printing) {
        const int irq=OS::DisableInterrupts();Counter& c=a->groups[group];
        const u32 after=block->blockSize;
        if(after>=before){const u32 n=after-before;c.live+=n;c.allocated+=n;a->live+=n;}
        else {const u32 n=before-after;c.live-=n;c.freed+=n;a->live-=n;}
        if(c.live>c.peak)c.peak=c.live;if(a->live>a->peak)a->peak=a->live;
        OS::RestoreInterrupts(irq);
    }
    return result;
}
void BulkReset(EGG::Heap* heap) {
    const int irq=OS::DisableInterrupts();
    for(u32 i=0;i<8;++i)if(arenas[i].heap==heap && !arenas[i].retired)
        {for(u32 g=0;g<17;++g){Counter& c=arenas[i].groups[g];c.freed+=c.live;c.live=0;}arenas[i].live=0;}
    OS::RestoreInterrupts(irq);
}
void Report(const char* phase,bool groups) {
    if(!enabled)return;
    // Copy one arena under the guard. OSReport/heap methods stay outside it.
    for(u32 i=0;i<8;++i) {
        const int irq=OS::DisableInterrupts();const Arena a=arenas[i];OS::RestoreInterrupts(irq);
        if(!a.raw)continue;
        if(a.retired){OS::Report("[MemOwner] gen=%u phase=%s arena=%s retired=1 peakPayload=%u\n",generation,phase,a.name,a.peak);continue;}
        if(!Readable(reinterpret_cast<u32>(a.raw),0x50) || a.raw->magic!=0x45585048)continue;
        const u32 capacity=reinterpret_cast<u32>(a.raw->endAddr)-reinterpret_cast<u32>(a.raw->startAddr);
        const u32 free=MEM::GetTotalFreeSizeForExpHeap(a.raw),largest=MEM::GetAllocatableSizeForExpHeapEx(a.raw,4);
        OS::Report("[MemOwner] gen=%u phase=%s arena=%s heap=%p capacity=%u free=%u largest=%u livePayload=%u peakPayload=%u failures=%u incomplete=%u\n",
            generation,phase,a.name,a.heap,capacity,free,largest,a.live,a.peak,a.failed,a.incomplete);
        if(groups)for(u32 g=0;g<17;++g){const Counter& c=a.groups[g];if(c.peak || c.allocs || c.frees)
            OS::Report("[MemOwner] gen=%u phase=%s arena=%s group=%u livePayload=%u peakPayload=%u allocated=%u freed=%u allocs=%u frees=%u\n",
                generation,phase,a.name,g,c.live,c.peak,c.allocated,c.freed,c.allocs,c.frees);}
    }
    // Measure the frame heap inside the 10 MiB sound reservation, not just its parent.
    Audio::GameHeap* audio=EPMemoryGameAudioHeap;
    if(audio && audio->isInitialized) {
        const void* frame=&audio->heap.frameHeap;
        MEM::HeapHandle raw=*reinterpret_cast<MEM::HeapHandle const*>(frame);
        if(Readable(reinterpret_cast<u32>(raw),0x3c) && raw->magic==0x46524d48) {
            OS::LockMutex(&audio->heap.mutex);
            const u32 free=EPMemorySoundFree(frame);
            const u32 capacity=reinterpret_cast<u32>(raw->endAddr)-reinterpret_cast<u32>(raw->startAddr);
            OS::UnlockMutex(&audio->heap.mutex);
            OS::Report("[MemOwner] gen=%u phase=%s nativeSoundFrame capacity=%u free=%u usedIncludingBookkeeping=%u\n",
                generation,phase,capacity,free,capacity-free);
        }
    }
}
kmBranch(0x80198d88, Allocate);
kmBranch(0x80199038, Free);
kmBranch(0x80198e38, Resize);
} } }
