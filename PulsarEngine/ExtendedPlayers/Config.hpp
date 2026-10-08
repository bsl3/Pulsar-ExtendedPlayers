#ifndef _PUL_EXTENDED_PLAYERS_CONFIG_
#define _PUL_EXTENDED_PLAYERS_CONFIG_
namespace Pulsar { namespace ExtendedPlayers { namespace Config {
static const unsigned PlayerCount = 24; // 12, 16, 18 or 24; offline VS/GP only.
static const bool LargeResults = false;
// Pack configuration: change these switches and rebuild Code.pul.
// Offline GP/VS only. The compile-time roster is selected before race setup.
// Online, time trials, teams and OTT retain their normal rosters.
// Zero requires exact rank resources; no misleading 13/14 texture substitution.
static const unsigned int PositionIconFallback = 0;
// Extended finishing fanfare: ceil(count * numerator / denominator).
// Default: the top half of the active roster.
static const unsigned int HappyFinishNumerator = 1;
static const unsigned int HappyFinishDenominator = 2;
// The native row is 602 x 34. Scale its parent uniformly to avoid stretching text;
// size the backgrounds and contents separately.
static const float ResultColumnX[2] = {-202.0f, 202.0f};
static const float ResultRowScale = 0.64f;
static const float ResultAspectFit43 = 0.78f;
static const float ResultRowHeight = 1.25f; // backgrounds +25% vs native
static const float ResultPortraitScale = 1.18f;
static const float ResultContentScale = 1.10f; // font width AND height
static const float ResultRowSpacing = 34.0f * ResultRowScale * ResultRowHeight;
static const float ResultTopY = 3.5f * ResultRowSpacing; // row-center coordinate
static const float ResultCenterY = 3.08403f; // native child-pane center

// LargeResults chooses the alternate profile. Parent scaling stays uniform.
struct ResultLayout {
    float columnX[2];
    float rowScale, aspectFit43, rowHeight, portraitScale, contentScale;
    float rowSpacing, topY;
    bool fillRow;
};
static const ResultLayout DefaultResultLayout = {
    {ResultColumnX[0], ResultColumnX[1]}, ResultRowScale, ResultAspectFit43,
    ResultRowHeight, ResultPortraitScale, ResultContentScale,
    ResultRowSpacing, ResultTopY, false
};
static const float LargerResultRowScale = 0.67f;
static const float LargerResultRowHeight = 1.90f;
static const float LargerResultRowSpacing = 34.0f * LargerResultRowScale * LargerResultRowHeight;
static const ResultLayout LargerResultLayout = {
    {-211.0f, 211.0f}, LargerResultRowScale, 0.76f, LargerResultRowHeight,
    1.32f, 1.24f, LargerResultRowSpacing, 3.5f * LargerResultRowSpacing, true
};

// Fit 12 rows per column inside the 456-unit viewport, with extra margins in 4:3.
static const float Result24DefaultSpacing = 34.0f * ResultRowScale * 1.35f;
static const ResultLayout DefaultResult24Layout = {
    {-202.0f, 202.0f}, ResultRowScale, 0.74f, 1.35f,
    1.18f, 1.10f, Result24DefaultSpacing, 5.5f * Result24DefaultSpacing, true
};
static const float Result24LargerSpacing = 34.0f * LargerResultRowScale * 1.55f;
static const ResultLayout LargerResult24Layout = {
    {-211.0f, 211.0f}, LargerResultRowScale, 0.72f, 1.55f,
    1.26f, 1.20f, Result24LargerSpacing, 5.5f * Result24LargerSpacing, true
};
// Intermediate rosters use both columns without reserving empty 24-player rows.
inline unsigned int ResultRowsPerColumn(unsigned int count) {
    return count > 12 ? (count + 1) / 2 : 8;
}
static const float RearGridStagger = 0.25f; // fraction of native pair spacing
// Compact grids use measured native row spacing, not per-racer zig-zag steps.
static const float CompactRearWidth = 0.65f;
static const float CompactRearGap = 0.50f;
static const float CompactGridWidth = 0.60f;
static const float CompactGridRowSpacing = 0.75f;
static const float CompactGridStagger = 0.06f;
static const float CompactGridMinimumSeparation = 400.0f; // world units
static const bool CPUParityLogging = true; // four snapshots per race, all CPUs
static const bool Enabled = true;
static const bool OfflineVS = true;
static const bool OfflineGP = true;
} } }
#endif
