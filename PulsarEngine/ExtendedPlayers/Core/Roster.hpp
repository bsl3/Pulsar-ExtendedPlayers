#ifndef PUL_ROSTER_CAPACITY_HPP
#define PUL_ROSTER_CAPACITY_HPP
#include <types.hpp>
namespace Pulsar { namespace ExtendedPlayers {
enum { VanillaCount = 12, Capacity = 24 };
static_assert(Capacity <= 24, "Audit native masks and resource limits before increasing capacity");

// Retail revision 0 is the original binary pair checked by tools/verify_regions.py.
inline bool IsSupportedRetailRegion() {
    const volatile u8* disc = reinterpret_cast<const volatile u8*>(0x80000000);
    return disc[0] == 'R' && disc[1] == 'M' && disc[2] == 'C'
        && (disc[3] == 'P' || disc[3] == 'E')
        && disc[4] == '0' && disc[5] == '1' && disc[7] == 0;
}
} }
#endif
