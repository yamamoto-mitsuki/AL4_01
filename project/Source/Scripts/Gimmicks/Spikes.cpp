#include "Spikes.h"

#include "Source/Scripts/IceCube.h"

SCRIPT(Spikes, "Gimmick");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void Spikes::Fields(ComponentUI& ui) {
	ui.Label("キューブが触れた瞬間に HP が減る。触れたままなら減らない（一度離れて、また触れたら減る）");
	ui.Field("Damage", damage_, Range(0.0f, 10.0f).Tip("減らす HP"));
	ui.Field("Bounce Speed", bounceSpeed_, AtLeast(0.0f).Tip("はねる速さ（m/s）。トゲの上（ローカルの +Y）の向きへ。壁や天井のトゲは、Rotation の Z で回すと、はねる向きも回る。0 ならはねない"));
}

//=============================================================================
// 当たり
//=============================================================================
void Spikes::OnTriggerEnter(GameObject other) {
	IceCube* cube = other.GetComponent<IceCube>();
	if (cube == nullptr) {
		return; // キューブでない
	}
	// 減らすのとはねるのは IceCube（同じフレームに何個のトゲに触れても、1回分にまとめる）
	cube->Damage(damage_, GetWorldUp(transform) * bounceSpeed_);
}