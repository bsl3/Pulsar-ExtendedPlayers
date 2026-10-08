#ifndef WII_LOADED_ITEM_SLOTS_RUNTIME
#define WII_LOADED_ITEM_SLOTS_RUNTIME
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
namespace Item { class ItemSlotData; }
namespace Pulsar { namespace ItemSlots {
void* GetFile(ArchiveMgr*, ArchiveSource, const char*, u32*);
// Zero means the extended asset was absent/invalid and native fallback is active.
u32 Columns();
// Use validated IDs across both roles/all columns for preload only; never change selection/capacity/dispatch.
u32 ResourceMask();
bool UsesExtendedTable(const Item::ItemSlotData* slot);
void TraceProbability(u32 column, const u32* table, const Item::ItemSlotData* slot);
} }
#endif
