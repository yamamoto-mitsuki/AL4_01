#include "JustCounter.h"

#include <format>

#include <HTTEngine/Diagnostics/LogManager.h>

SCRIPT(JustCounter, "Player");


//=============================================================================
// ジャスト反撃
//=============================================================================
// ===== 開始 =====
void JustCounter::Begin(const JustCandidate& just) { LogManager::Log("じゃすとー！"); }