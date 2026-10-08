#include <ItemSlots/Runtime.hpp>
#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <ItemSlots/Config.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <core/rvl/OS/OS.hpp>

extern "C" {
void WISNativeInit(Item::ItemSlotData*);
const void* WISNativeProcess(Item::ItemSlotData*, const void*,
    Item::ItemSlotData::Probabilities*, bool, bool);
}
namespace Pulsar { namespace ItemSlots {
// The C++ roulette list must match the native allocation size.
static const u32 NativeCount = 19, ItemCount = 19, FileItemCount = 32;
struct VisualPool { u32 itemCount; ItemId items[19]; };
size_assert(VisualPool, sizeof(Item::ItemSlotData::RouletteItems));
static bool IsValid(ItemId id) { return id >= 0 && id < 19; }
// Use archive pointers only during race initialization; scene teardown invalidates them.
struct Extension {
    const u8* human;
    const u8* cpu;
    u32 columns, rows, version;
    const u8* ids;
};
static u32 installedColumns;
static_assert(FileItemCount<=32,"extend the presentation demand bitset for IDs32+");
static u32 resourceMask = ~0u;
static bool loading;
static const void* selectedFile;
static u32 selectedBytes;
static const char* selectedSource;
static Item::ItemSlotData* installedSlot;
static const u16* installedTables[2];
static u32 loggedColumns[2];
static u32 originalHashes[2][Config::Capacity];
static u8 authoredBananas[2][Config::Capacity];
static bool replacementReported;
static u32 reloadReported;
static bool lookupReported;
static bool specialReported;

static bool Enabled(u32 count) {
    return count > Config::OverrideCourseAbove && count <= Config::Capacity;
}

static u32 Count() {
    if(!Racedata::sInstance) return 0;
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const GameMode mode = scenario.settings.gamemode;
    if(mode != MODE_VS_RACE && mode != MODE_GRAND_PRIX) return 0;
    return scenario.playerCount > 12 && !ExtendedPlayers::IsActive() ? 0 : scenario.playerCount;
}
static u32 Read32(const u8* p) {
    return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3];
}
// Return null for a valid file, otherwise a reason to print in the console.
static const char* Parse(const void* file, u32 bytes, u32 count, Extension& result) {
    if(!file) return "file missing";
    if(!bytes) return "empty file";
    if(!Enabled(count)) return "extended race condition inactive";
    const u8* data = static_cast<const u8*>(file);
    if(data[0] != 6 && data[0] != 12) return "native table count must be 6 or 12";
    u32 offset = 1;
    for(u32 t = 0; t < data[0]; ++t) {
        if(offset + 2 > bytes) return "truncated native header";
        const u32 cols = data[offset], rows = data[offset + 1];
        if(!cols || rows != 19 || (t < 5 && cols != 12) || (t == 5 && cols != 16)
            || (t >= 6 && cols != 3)) return "invalid native prefix dimensions";
        const u32 size = 2 + cols * rows;
        if(size > bytes - offset) return "truncated native table";
        if(t < 5) for(u32 c = 0; c < cols; ++c) {
            u32 sum = 0;
            for(u32 r = 0; r < rows; ++r) sum += data[offset + 2 + r * cols + c];
            if(!sum) return "empty native race column";
        }
        offset += size;
    }
    if(bytes - offset < 16) return "missing WISL extension";
    const u8* header = data + offset;
    const u32 cols = header[6], rows = header[7], version = header[5];
    if(Read32(header) != 0x5749534c) return "WISL magic missing";
    if(header[4] || (version != 1 && version != 2)) return "unsupported WISL version";
    if(cols < count || cols < 12 || cols > Config::Capacity) return "insufficient/unsupported position columns";
    if(header[8] != 2 || header[9] || header[10] || header[11]
        || (version == 1 && rows != 19) || !rows || rows > FileItemCount)
        return "invalid WISL dimensions/flags";
    const u32 directoryBytes = version == 2 ? rows * 2 : 0;
    const u32 size = 2 + cols * rows;
    if(Read32(header + 12) != directoryBytes + 2 * size
        || bytes - offset != 16 + directoryBytes + 2 * size)
        return "WISL payload length mismatch";
    const u8* ids = version == 2 ? header + 16 : static_cast<const u8*>(nullptr);
    bool seen[FileItemCount] = {false};
    for(u32 r = 0; r < rows; ++r) {
        const u32 id = ids ? (u32(ids[r*2]) << 8) | ids[r*2+1] : r;
        if(id >= FileItemCount)
            return "unregistered/reserved item ID in WISL directory";
        if(seen[id]) return "duplicate item ID in WISL directory";
        seen[id] = true;
    }
    const u8* tables = header + 16 + directoryBytes;
    for(u32 t = 0; t < 2; ++t) {
        const u8* table = tables + t * size;
        if(table[0] != cols || table[1] != rows) return "WISL table dimensions disagree";
        for(u32 c = 0; c < cols; ++c) {
            u32 sum = 0, activeSum = 0;
            for(u32 r = 0; r < rows; ++r) {
                const u32 id = ids ? (u32(ids[r*2]) << 8) | ids[r*2+1] : r;
                if(id >= NativeCount && table[2 + r * cols + c])
                    return "custom item weights require a custom-item runtime";
                sum += table[2 + r * cols + c];
                if(IsValid(static_cast<ItemId>(id))) activeSum += table[2 + r * cols + c];
            }
            if(sum != 200) return t ? "CPU column total is not 100%" : "Player column total is not 100%";
            if(!activeSum)return "column contains no enabled item weights";
        }
    }
    result.human = tables; result.cpu = tables + size;
    result.columns = cols; result.rows = rows; result.ids = ids; result.version = version;
    return nullptr;
}
// Reordered file rows become native item indices. Unimplemented rows must be zero.
static void ExpandRaw(const Extension& extension, bool cpu, u8* dest) {
    const u32 cols = extension.columns;
    memset(dest, 0, 2 + cols * ItemCount);
    dest[0] = cols; dest[1] = ItemCount;
    const u8* source = cpu ? extension.cpu : extension.human;
    for(u32 r = 0; r < extension.rows; ++r) {
        const u32 id = extension.ids ? (u32(extension.ids[r*2]) << 8) | extension.ids[r*2+1] : r;
        if(!IsValid(static_cast<ItemId>(id)))continue;
        memcpy(dest + 2 + id * cols, source + 2 + r * cols, cols);
    }
}
static const void* CommonFile(ArchiveMgr* archive, u32 count, u32& size,
    Extension& extension, const char*& source, bool report) {
    size = 0;
    if(!archive || !archive->archivesHolders) {
        if(report) OS::Report("[ItemSlot24] failure=ArchiveMgr/common holders unavailable\n");
        return nullptr;
    }
    const ArchivesHolder* common = archive->archivesHolders[ARCHIVE_HOLDER_COMMON];
    if(!common || !common->archives) {
        if(report) OS::Report("[ItemSlot24] failure=common archive holder unavailable\n");
        return nullptr;
    }
    // GetFile searches last-to-first (8052A784..8052A7E4). Select CommonAssets explicitly,
    // not ItemSlot.bin or a kart archive with a matching name.
    const u32 indices[2] = {2, 0};
    const char* sources[2] = {"CommonAssets.szs", "common.szs"};
    for(u32 i = 0; i < 2; ++i) {
        const u32 index = indices[i];
        const void* file = nullptr;
        size = 0;
        if(report) OS::Report("[ItemSlot24] common archive index=%u available=%u status=%u\n",
            index, index < common->archiveCount,
            index < common->archiveCount ? common->archives[index].status : 0);
        if(index < common->archiveCount)
            file = common->archives[index].GetFile(Config::ExtendedFile, &size);
        source = sources[i];
        if(report) OS::Report("[ItemSlot24] resource=%s/%s found=%u bytes=%u\n",
            source, Config::ExtendedFile, file != nullptr, size);
        if(!file) continue;
        const char* error = Parse(file, size, count, extension);
        if(report) OS::Report("[ItemSlot24] validation=%s reason=%s\n",
            error ? "FAIL" : "PASS", error ? error : "WISL validated, independent Player/CPU item IDs");
        // Reject an invalid explicit file rather than silently hiding edits behind defaults.
        return error ? nullptr : file;
    }
    return nullptr;
}
static void* StockProbabilities(ArchiveMgr* archive, u32* length) {
    // Read the vanilla Common archive directly; course/custom files cannot replace this fallback.
    if(length) *length = 0;
    if(!archive || !archive->archivesHolders) return nullptr;
    const ArchivesHolder* common = archive->archivesHolders[ARCHIVE_HOLDER_COMMON];
    if(!common || !common->archives || !common->archiveCount) return nullptr;
    return common->archives[0].GetFile("ItemSlot.bin", length);
}
void* GetFile(ArchiveMgr* archive, ArchiveSource source, const char* name, u32* length) {
    const u32 count = Count();
    if((source == ARCHIVE_HOLDER_COURSE || source == ARCHIVE_HOLDER_COMMON) && (Enabled(count) || (loading && selectedFile && source == ARCHIVE_HOLDER_COMMON))) {
        u32 bytes = 0; Extension extension; const char* origin = nullptr;
        const void* file = loading ? selectedFile : CommonFile(archive, count, bytes, extension, origin, false);
        if(loading) bytes = selectedBytes;
        if(file) {
            if(!lookupReported) {
                OS::Report("[ItemSlot24] native lookup source=%u path=%s redirected=%s/%s; course SLT cannot win\n",
                    source, name, loading ? selectedSource : origin, Config::ExtendedFile);
                lookupReported = true;
            }
            if(length) *length = bytes;
            return const_cast<void*>(file);
        }
        return StockProbabilities(archive, length);
    }
    u32 bytes = 0;
    void* file = archive->GetFile(source, name, &bytes);
    if(length) *length = bytes;
    return file;
}
static u32 HashColumn(const u16* values) {
    u32 hash = 2166136261u;
    for(u32 r = 0; r < ItemCount; ++r) hash = (hash ^ values[r]) * 16777619u;
    return hash;
}
static void Init(Item::ItemSlotData* slot) {
    installedColumns = 0;
    resourceMask = ~0u;
    selectedFile = nullptr; selectedBytes = 0;
    const u32 count = Count();
    Extension extension;
    installedSlot = nullptr;
    installedTables[0] = installedTables[1] = nullptr;
    loggedColumns[0] = loggedColumns[1] = 0;
    replacementReported = lookupReported = specialReported = false; reloadReported = 0;
    selectedSource = "none";
    OS::Report("[ItemSlot24] init count=%u extended=%u rule=count>12 offline VS/GP\n", count, Enabled(count));
    if(Enabled(count))
        selectedFile = CommonFile(ArchiveMgr::sInstance, count, selectedBytes, extension, selectedSource, true);
    loading = true;
    WISNativeInit(slot); // original flags, special-box tables, timers and setup
    loading = false;
    if(selectedFile) {
        const u32 cols = extension.columns;
        // Allocate the tables in the scene heap; do not keep them after the scene unloads.
        delete[] slot->playerChances.probabilities;
        delete[] slot->cpuChances.probabilities;
        delete[] slot->unknown;
        slot->playerChances.rowCount = slot->cpuChances.rowCount = cols;
        slot->playerChances.probabilities = new u16[cols * ItemCount];
        slot->cpuChances.probabilities = new u16[cols * ItemCount];
        
        VisualPool* pools = new VisualPool[cols];
        memset(pools, 0, cols * sizeof(VisualPool));
        slot->unknown = reinterpret_cast<Item::ItemSlotData::RouletteItems*>(pools);
        slot->normalBoxes->itemCount = 0;
        // Create expanded pools before ProcessTable builds modified columns and visual lists.
        u8 rawTable[2 + Config::Capacity * ItemCount];
        ExpandRaw(extension, false, rawTable);
        WISNativeProcess(slot, rawTable, &slot->playerChances, true, false);
        ExpandRaw(extension, true, rawTable);
        WISNativeProcess(slot, rawTable, &slot->cpuChances, false, false);
        installedColumns = cols;
        installedSlot = slot;
        installedTables[0] = slot->playerChances.probabilities;
        installedTables[1] = slot->cpuChances.probabilities;
        const u8* raw[2] = {extension.human, extension.cpu};
        resourceMask = 0;
        for(u32 role = 0; role < 2; ++role) for(u32 c = 0; c < cols; ++c) {
            authoredBananas[role][c] = 0;
            for(u32 r = 0; r < extension.rows; ++r) {
                const u32 id = extension.ids ? (u32(extension.ids[r*2]) << 8) | extension.ids[r*2+1] : r;
                if(raw[role][2+r*cols+c])resourceMask |= 1u<<id;
                if(id == TRIPLE_BANANA) authoredBananas[role][c] = raw[role][2 + r * cols + c];
            }
            originalHashes[role][c] = HashColumn(installedTables[role] + c * ItemCount);
        }
        OS::Report("[ItemSlot24] ACTIVATED count=%u columns=%u source=%s/%s courseOverride=count>12 player=%p cpu=%p\n",
            count, cols, selectedSource, Config::ExtendedFile, installedTables[0], installedTables[1]);
    }
    else OS::Report("[ItemSlot24] inactive reason=%s; source=%s\n",
        Enabled(count) ? "missing/invalid ItemSlot24.bin (see resource/validation above)" : "extended condition inactive",
        Enabled(count) ? "stock native items (no normal custom file/SLT)" : count ? "native item tables" : "native non-race mode");
    selectedFile = nullptr; selectedBytes = 0;
}
u32 ResourceMask() { return installedColumns ? resourceMask : ~0u; }
u32 Columns() { return Count() ? installedColumns : 0; }
bool UsesExtendedTable(const Item::ItemSlotData* slot) {
    return !loading && Count() && installedColumns && slot == installedSlot;
}
void TraceProbability(u32 column, const u32* table, const Item::ItemSlotData* slot) {
    if(!Count() || !installedColumns || slot != installedSlot) return;
    if(table == reinterpret_cast<const u32*>(&slot->specialChances)) {
        if(specialReported || column >= slot->specialChances.rowCount || !slot->specialChances.probabilities) return;
        specialReported = true;
        const u16* values = slot->specialChances.probabilities + column * ItemCount;
        u32 sum = 0;
        for(u32 r = 0; r < ItemCount; ++r) sum += values[r];
        OS::Report("[ItemSlot24] roulette table=Special set=%u TripleBananas processed=%u/%u; uses asset prefix special sets, not placement columns\n",
            column + 1, values[18], sum);
        return;
    }
    const bool cpu = table == reinterpret_cast<const u32*>(&slot->cpuChances);
    const Item::ItemSlotData::Probabilities& current = cpu ? slot->cpuChances : slot->playerChances;
    const char* role = cpu ? "CPU" : "Player";
    if(current.rowCount != installedColumns || current.probabilities != installedTables[cpu]) {
        if(!replacementReported) OS::Report("[ItemSlot24] ERROR table replaced role=%s columns=%u expected=%u pointer=%p expected=%p\n",
            role, current.rowCount, installedColumns, current.probabilities, installedTables[cpu]);
        replacementReported = true; return;
    }
    if(column >= current.rowCount) return;
    const u16* values = current.probabilities + column * ItemCount;
    if(HashColumn(values) != originalHashes[cpu][column] && !replacementReported) {
        OS::Report("[ItemSlot24] WARNING probabilities changed after activation role=%s column=%u\n", role, column + 1);
        replacementReported = true;
    }
    const u32 bit = 1u << column;
    if(loggedColumns[cpu] & bit) return;
    loggedColumns[cpu] |= bit; // At most one line per role/column per initialization.
    u32 sum = 0;
    for(u32 r = 0; r < ItemCount; ++r) sum += values[r];
    OS::Report("[ItemSlot24] roulette table=%s place=%u column=%u/%u TripleBananas raw=%u/200 processed=%u/%u (%.1f%%); native eligibility/capacity filters still apply\n",
        role, column + 1, column + 1, current.rowCount, authoredBananas[cpu][column],
        values[18], sum, sum ? 100.0f * values[18] / sum : 0.0f);
}
// Reload the extended probabilities if native setup writes the prefix again.
// Return the prefix's next-table pointer so special and battle tables are still read correctly.
static const void* Process(Item::ItemSlotData* slot, const void* raw,
    Item::ItemSlotData::Probabilities* probabilities, bool roulette, bool special) {
    if(!loading && Count() && installedColumns && slot == installedSlot && !special
        && (probabilities == &slot->playerChances || probabilities == &slot->cpuChances)) {
        u32 bytes = 0; Extension extension; const char* origin = nullptr;
        if(CommonFile(ArchiveMgr::sInstance, Count(), bytes, extension, origin, false)
            && extension.columns == installedColumns) {
            const bool cpu = probabilities == &slot->cpuChances;
            if(roulette) {
                slot->normalBoxes->itemCount = 0;
                memset(slot->unknown, 0, installedColumns * sizeof(VisualPool));
            }
            u8 rawTable[2 + Config::Capacity * ItemCount];
            ExpandRaw(extension, cpu, rawTable);
            WISNativeProcess(slot, rawTable, probabilities, roulette, false);
            for(u32 c = 0; c < installedColumns; ++c)
                originalHashes[cpu][c] = HashColumn(probabilities->probabilities + c * ItemCount);
            if(!(reloadReported & (1u << static_cast<u32>(cpu)))) OS::Report("[ItemSlot24] native table reload role=%s kept extended columns=%u; prefix/SLT replacement blocked\n",
                cpu ? "CPU" : "Player", installedColumns);
            reloadReported |= 1u << static_cast<u32>(cpu);
            const u8* prefix = static_cast<const u8*>(raw);
            return prefix + 2 + prefix[0] * prefix[1];
        }
    }
    return WISNativeProcess(slot, raw, probabilities, roulette, special);
}
static void PostProcess(Item::ItemSlotData* slot, Item::ItemSlotData::Probabilities* probabilities) {
    // Keep the file's separate position columns; item-mode modifiers have already run.
    if(!loading && Count() && installedColumns && slot == installedSlot) return;
    slot->PostProcessVSTable(probabilities);
}
kmCall(0x807ba7d8, Init);
kmCall(0x807ba904, Init);
// Standalone course and common-file hooks are installed below; no BinLoader edits are needed.
kmCall(0x807bb010, GetFile);
kmCall(0x807bb108, GetFile);
kmCall(0x807bb1dc, GetFile);
kmCall(0x807bb51c, GetFile);
kmCall(0x807bbac4, GetFile);
kmCall(0x807bbb38, GetFile);
kmCall(0x807bbdb4, GetFile);
kmCall(0x807bbf30, GetFile);
kmCall(0x807bc038, GetFile);
kmCall(0x807bb078, Process);
kmCall(0x807bb090, Process);
kmCall(0x807bb0a8, Process);
kmCall(0x807bb160, Process);
kmCall(0x807bb178, Process);
kmCall(0x807bb1a0, Process);
kmCall(0x807bb21c, Process);
kmCall(0x807bb234, Process);
kmCall(0x807bb27c, Process);
kmCall(0x807bb584, Process);
kmCall(0x807bb59c, Process);
kmCall(0x807bb5b4, Process);
kmCall(0x807bbbc0, Process);
kmCall(0x807bbc40, Process);
kmCall(0x807bbc58, Process);
kmCall(0x807bbc70, Process);
kmCall(0x807bbcec, Process);
kmCall(0x807bbd28, Process);
kmCall(0x807bbd40, Process);
kmCall(0x807bbe1c, Process);
kmCall(0x807bbe34, Process);
kmCall(0x807bbe4c, Process);
kmCall(0x807bbf88, Process);
kmCall(0x807bbfa0, Process);
kmCall(0x807bbfc8, Process);
kmCall(0x807bc078, Process);
kmCall(0x807bc090, Process);
kmCall(0x807bc0d8, Process);
// ExtendedPlayers already owns six postprocess hooks and checks UsesExtendedTable.
kmCall(0x807bbe58, PostProcess);
kmCall(0x807bbe64, PostProcess);
kmCall(0x807bbfd4, PostProcess);
kmCall(0x807bbfe0, PostProcess);
kmCall(0x807bb128, GetFile);
kmCall(0x807bb030, GetFile);
kmCall(0x807bb200, GetFile);
kmCall(0x807bb53c, GetFile);
kmCall(0x807bbb58, GetFile);
kmCall(0x807bbdd4, GetFile);
kmCall(0x807bbf50, GetFile);
} }
