#pragma once
#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/Vector3.h>

// 前方宣言
struct RigidbodyComponent;

/// <summary>
/// 動いた長さに合わせて、粒を残していく（通った跡）。Particle System を付けた Entity に付ける（氷のキューブの足元の子「FootTrail」など）
/// <para>Particle System は Play On Start を外し、Rate Over Time・Burst Count を 0 にする（出すのは全部ここ。ParticleSystem::Emit で出す）</para>
/// <para>時間ではなく「進んだ長さ」で出すので、止まっていると出ない・速いほど跡が長くなる（Unity の Rate over Distance と同じ考え方）</para>
/// <para>Particle System の Simulation Space は World にする（Local だと、粒が一緒に付いてきて跡にならない）</para>
/// <para>Ground Only：自分か親の Rigidbody が地面に乗っている（Is Grounded）間だけ出す（走ると足元から粒が舞う。跳んでいる間は出ない）</para>
/// </summary>
class DistanceTrail : public Script {
public:
	void Start() override;
	void Update() override;
	void Fields(ComponentUI& ui) override;

private:
	// 地面を見る Rigidbody（自分に無ければ親の。どちらにも無ければ nullptr）
	const RigidbodyComponent* FindRigidbody() const;

	// ===== Inspector に出す・保存する値 =====
	float perMeter_ = 8.0f;  // 1m 進むごとに出す粒の数
	float minSpeed_ = 1.0f;  // これより遅いときは出さない（m/s。止まりかけ・溶けて沈むときに出さない）
	float maxStep_ = 3.0f;   // 1フレームにこれより長く動いたら、出さない（m。やり直し・場所を飛んだときに、線のように粒が並ばないように）
	bool groundOnly_ = true; // 地面に乗っている間だけ出す（自分か親の Rigidbody の Is Grounded を見る）

	// ===== 実行中だけの値（保存しない）=====
	Vector3 previousPosition_ = {0.0f, 0.0f, 0.0f}; // 前のフレームの位置（ワールド）
	float carry_ = 0.0f;                            // 出しきれなかった端数（0〜1 個分。次のフレームへ持ち越す）
};