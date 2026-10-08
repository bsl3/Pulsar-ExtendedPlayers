#ifndef PUL_EXTENDED_POSITION_TEXTURE
#define PUL_EXTENDED_POSITION_TEXTURE
#include <core/rvl/gx/GX.hpp>
namespace Pulsar { namespace ExtendedPlayers {
// Optional numbered TPL assets take precedence; missing assets use legible digits.
const GX::TexObj* NumericPositionTexture(unsigned int rank);
} }
#endif
