#include "JumpRing.h"

#include <MyEngine/Systems/ParticleSystem.h>

#include "Source/Scripts/IceCube.h"

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