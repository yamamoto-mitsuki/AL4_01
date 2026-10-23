#pragma once
#include <MyEngine/ECS/Script.h>

/// <summary>
/// トゲ（1列）。Collider（Is Trigger）を付けた Entity に付ける。見た目は Polygon Shape のギザギザなど（Collider の Shape を Polygon にすると、当たりも同じ形になる）
/// <para>氷のキューブが触れた瞬間に、HP を Damage だけ減らして、トゲの上（ローカルの +Y）へはねさせる</para>
/// <para>触れたままなら減らない。一度離れて、また触れたら、また減る（無敵時間は無い）。1列は1つの Trigger にする（分けると、歩くだけで何回も減る）</para>
/// <para>同じフレームに2つのトゲに触れても、減るのは1回分（IceCube が1フレームにまとめる）</para>
/// </summary>
class Spikes : public Script {
public:
	void Fields(ComponentUI& ui) override;
	void OnTriggerEnter(GameObject other) override; // キューブが触れた瞬間

private:
	// ===== Inspector に出す・保存する値 =====
	int damage_ = 1;           // 減らす HP
	float bounceSpeed_ = 8.0f; // はねる速さ（m/s。トゲの上の向きへ。0 ならはねない）
};