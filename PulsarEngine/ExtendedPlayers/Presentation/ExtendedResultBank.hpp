#ifndef PUL_EXTENDED_RESULT_BANK_HPP
#define PUL_EXTENDED_RESULT_BANK_HPP
class CtrlRaceResult;
namespace Pulsar { namespace ExtendedPlayers {
// Copy the original row sizes only; keep each page's scores and ordering separate.
void CopyResultBankMetrics();
void RecordResultBankRowCost(unsigned rank, unsigned bytes);
} }
#endif
