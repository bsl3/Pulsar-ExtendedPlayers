#ifndef WII_LOADED_ITEM_SLOTS_CONFIG
#define WII_LOADED_ITEM_SLOTS_CONFIG
namespace Pulsar { namespace ItemSlots { namespace Config {
static const unsigned Capacity = 24;
// Put the editor's exported file at the root of CommonAssets.szs.
// Race/Common.szs is supported only when CommonAssets.szs has no such file.
// A separate name keeps the ordinary <=12-player ItemSlot.bin untouched.
static const char ExtendedFile[] = "ItemSlot24.bin";
static const char CourseFile[] = "ItemSlotTable/ItemSlotTable.slt";
static const unsigned OverrideCourseAbove = 12;
} } }
#endif
