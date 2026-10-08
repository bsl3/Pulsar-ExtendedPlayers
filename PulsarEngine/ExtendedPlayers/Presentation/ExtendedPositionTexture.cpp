#include <ExtendedPlayers/ExtendedPlayers.hpp>
#include <ExtendedPlayers/Presentation/ExtendedPositionTexture.hpp>
#include <PulsarSystem.hpp>
#include <core/rvl/OS/OSCache.hpp>
#include <core/egg/mem/Heap.hpp>
extern "C" void GXInitTexObj(GX::TexObj*, void*, u16, u16, GX::TexFmt,
    GX::TexWrapMode, GX::TexWrapMode, bool);
namespace Pulsar { namespace ExtendedPlayers {
static GX::TexObj textures[Capacity - VanillaCount];
static void* images[Capacity - VanillaCount];
// Generate missing rank numerals once from 5x7 glyphs; no per-frame allocation or game artwork.
static const u8 glyphs[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
};
static bool DigitPixel(u32 rank, int x, int y) {
    // Two 20x28 glyphs centered in the native 64x64 position texture.
    if(y < 18 || y >= 46) return false;
    u32 digit;
    if(x >= 10 && x < 30) { digit = rank / 10; x -= 10; }
    else if(x >= 34 && x < 54) { digit = rank % 10; x -= 34; }
    else return false;
    return (glyphs[digit][(y - 18) / 4] & (1u << (4 - x / 4))) != 0;
}
const GX::TexObj* NumericPositionTexture(u32 rank) {
    if(rank <= VanillaCount || rank > Capacity) return nullptr;
    const u32 index = rank - VanillaCount - 1;
    if(!images[index]) {
        u8* image = EGG::Heap::alloc<u8>(64 * 64 * 4, 32, System::sInstance->heap);
        if(!image) return nullptr;
        for(int y = 0; y < 64; ++y) for(int x = 0; x < 64; ++x) {
            const bool fill = DigitPixel(rank, x, y);
            bool edge = false;
            if(!fill) for(int dy = -1; dy <= 1; ++dy) for(int dx = -1; dx <= 1; ++dx)
                edge |= DigitPixel(rank, x + dx, y + dy);
            // GX RGBA8 is 4x4 tiled: sixteen AR pairs then sixteen GB pairs.
            const u32 tile = ((y / 4) * 16 + x / 4) * 64;
            const u32 pixel = ((y % 4) * 4 + x % 4) * 2;
            const u8 color = fill ? 255 : 0;
            image[tile + pixel] = fill || edge ? 255 : 0;
            image[tile + pixel + 1] = color;
            image[tile + 32 + pixel] = color;
            image[tile + 32 + pixel + 1] = color;
        }
        OS::DCFlushRange(image, 64 * 64 * 4);
        GXInitTexObj(&textures[index], image, 64, 64, GX::GX_TF_RGBA8,
            GX::GX_CLAMP, GX::GX_CLAMP, false);
        images[index] = image; // System-owned texture intentionally survives races.
    }
    return &textures[index];
}
} }
