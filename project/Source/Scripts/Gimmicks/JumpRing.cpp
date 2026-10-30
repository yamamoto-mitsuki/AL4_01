#include "JumpRing.h"

#include <MyEngine/Systems/Effects/ParticleSystem.h>

#include "Source/Scripts/Effects/GameRumble.h"
#include "Source/Scripts/IceCube.h"

namespace {
// 使ったときのコントローラーの振動（強さは水色の円・緑の円で同じ。だんだん大きくはしない）
constexpr float kRumblePower = 0.9f; // 強さ（0〜1。左右のモーターに同じ値）
constexpr float kRumbleTime = 0.15f; // 震える秒数（水色の円。緑の円は LaunchOrb::GetRumbleTime で長く）
} // namespace

//=============================================================================
// 使ったときに震える秒数（派生クラスで変えないなら、水色の円と同じ）
//=============================================================================
float JumpRing::GetRumbleTime() const { return kRumbleTime; }

//=============================================================================
// Inspector に出す・保存する値（派生クラスの Fields から、最初に呼ぶ）
//=============================================================================
void JumpRing::Fields(ComponentUI& ui) {
	ui.Field(
	    "Gather Particle", gatherParticleName_,
	    Tip("ふだん真ん中へ吸い寄せられる粒の Particle System の名前（直下の子。Converge を付けて Play On Start）。使ったら消えて、キューブが出たらまた流れる。無ければ何もしない"));
	ui.Field("Burst Particle", burstParticleName_, Tip("使ったときに外へはじけ飛ぶ粒の Particle System の名前（直下の子。Play On Start を外して、Burst で出す）。無ければ何もしない"));
}

//=============================================================================
// 使う・戻る
//=============================================================================
void JumpRing::Use() {
	isUsed_ = true;
	// --- 粒：吸い寄せられていた粒を消して、外へはじけ飛ばす ---
	if (const GameObject gather = gameObject.FindChild(gatherParticleName_)) {
		ParticleSystem::Stop(gather.handle, true); // true：出ている粒も消す（集まっていた粒が、はじけた粒に入れ替わる）
	}
	if (const GameObject burst = gameObject.FindChild(burstParticleName_)) {
		ParticleSystem::Play(burst.handle); // Burst Count の数を一度に出す
	}
	// --- コントローラー：一定の強さで短く震わせる（止めるのは GameRumble が数えて）---
	GameRumble::Play(kRumblePower, GetRumbleTime());
	OnUse(); // ほかの見た目（派生クラス）
}

void JumpRing::OnTriggerExit(GameObject other) {
	// キューブが輪から出たら、また使える（入り直すと使える）
	if (other.GetComponent<IceCube>() == nullptr || !isUsed_) {
		return;
	}
	isUsed_ = false;
	// --- 粒：また吸い寄せ始める ---
	if (const GameObject gather = gameObject.FindChild(gatherParticleName_)) {
		ParticleSystem::Play(gather.handle);
	}
}