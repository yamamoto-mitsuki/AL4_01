#pragma once
#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/Vector2.h>

/// <summary>
/// 風の範囲（Wind Zone）の中に、風に流される粒を出す。Wind Zone と Particle System を両方付けた Entity に付ける
/// <para>Particle System は Play On Start を外す（出すのはここ。Rate Over Time の数だけ、毎秒 ParticleSystem::Emit で出す）</para>
/// <para>粒の「動き」はここが毎フレーム Particle System に書く：Shape を Box にして Wind Zone の Size に合わせる・風の速さで流れるように
/// Gravity と Drag を決める・範囲を流れ切るくらいの Lifetime にする。Inspector でそこを変えても、ここが上書きする</para>
/// <para>粒の「見た目」と量（Rate Over Time・Size・Color・End Color・Texture など）は、Particle System の Inspector で決める</para>
/// <para>風のゆれ（Gust）も粒の速さに入る（WindZoneSystem::Sample で、範囲の真ん中の風を読む）。Wind Zone の Enabled を外すと、出すのをやめる</para>
/// <para>Show Gust：今の風の強さ（Wind Zone の Speed の何倍か）で、筋の長さ（Stretch）・明るさ（Intensity）・出る数を変える。明るさは強さの2乗。弱まると短く暗く少なく、強まると長く明るく多くなる</para>
/// </summary>
class WindParticles : public Script {
public:
	void Start() override;
	void Update() override;
	void Fields(ComponentUI& ui) override;

private:
	// ===== Inspector に出す・保存する値 =====
	float followRate_ = 4.0f;         // 粒が風の速さに追いつく強さ（1/秒。大きいほど、出た瞬間から風の速さで流れる）
	Vector2 lifeRate_ = {0.4f, 0.8f}; // 粒の寿命（範囲を風の速さで流れ切る秒数に掛ける。Min〜Max）
	bool showGust_ = false;           // 風の強さで、筋の長さ・明るさ・出る数を変える（強い風・弱い風が見て分かるように）

	// ===== 実行中だけの値（保存しない）=====
	float carry_ = 0.0f;         // 出しきれなかった端数（0〜1 個分。次のフレームへ持ち越す）
	float baseStretch_ = 1.0f;   // 最初の Stretch（Show Gust はこれに強さを掛ける）
	float baseIntensity_ = 1.0f; // 最初の Intensity（同じ）
};