#ifndef PUL_MEMORY_ACCOUNTING_HPP
#define PUL_MEMORY_ACCOUNTING_HPP
#include <core/egg/mem/Heap.hpp>
namespace Pulsar { namespace ExtendedPlayers { namespace MemoryAccounting {
// These diagnostics only observe allocations; they do not move, resize, free or retry them.
void Begin(u32 generation, bool enabled);
void Watch(const char* name, EGG::Heap* heap);
void Report(const char* phase, bool groups);
void BulkReset(EGG::Heap* heap);
} } }
#endif
