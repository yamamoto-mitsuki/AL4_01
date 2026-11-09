#pragma once
#include <string>

#include <HTTEngine/ECS/Script.h>
#include <HTTEngine/Systems/Combat/HitSystem.h>

//=============================================================================
// ジャスト反撃の流れを受け持つ
//=============================================================================

/// <summary>
/// ジャスト反撃の流れ
/// </summary>
class JustCounter : public Script {
public:
	// ===== 他のScriptから読む =====
	/// <summary>
	/// ジャスト反撃を開始する
	/// </summary>
	/// <param name="just">よけた攻撃</param>
	void Begin(const JustCandidate& just);

	bool IsActive() const { return false; } // 反撃の流れの途中か


private:
	
};