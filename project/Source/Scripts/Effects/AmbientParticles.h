#pragma once
#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/Vector2.h>
#include <MyEngine/Math/Vector3.h>

/// <summary>
/// ゲーム中の画面に、雪や蛍のような粒をふわふわ漂わせる（飾り）。Particle System を付けた、親の無い Entity に付ける
/// <para>毎フレーム、この Entity をメインカメラの前（Depth の真ん中）へ動かし、Particle System の Box を「見えている範囲」の大きさにする
/// （カメラが動いても、いつも画面の中に粒が出る）</para>
/// <para>Simulation Space は World にする（出た粒はその場に残る。カメラが動くと、奥の粒はゆっくり・手前の粒は速く流れて、奥行きが出る）</para>
/// <para>ふわふわの動き：全部の粒が「Drift ＋ 揺れ」の速さへゆっくり近づく（Gravity ＝ 目標の速さ × Drag。WindParticles と同じ考え方）。
/// 揺れは Sway の大きさで、Sway Period 秒かけて左右・上下に行き来する。出たときの向き（Particle System の Speed）がしばらく残るので、粒ごとに少しずつ違う動きになる</para>
/// <para>粒の見た目と量（Rate Over Time・Lifetime・Speed・Size・Color・Intensity など）は Particle System の Inspector で決める。
/// Shape・Box Size・Space・Gravity・Drag はここが毎フレーム上書きする</para>
/// </summary>
class AmbientParticles : public Script {
public:
	void Update() override;
	void Fields(ComponentUI& ui) override;

private:
	// ===== Inspector に出す・保存する値 =====
	Vector2 depth_ = {6.0f, 26.0f};         // 粒が出る奥行き（カメラから何 m。Min〜Max）
	float margin_ = 1.2f;                   // 見えている範囲の何倍の広さに出すか（端から粒が流れ込んでくるように、少し広く）
	Vector3 drift_ = {0.15f, -0.25f, 0.0f}; // 漂う向きと速さ（m/s。雪なら -Y、蛍なら少し +Y）
	Vector2 sway_ = {0.5f, 0.2f};           // 揺れの大きさ（m/s。X・Y）
	float swayPeriod_ = 7.0f;               // 揺れの1往復の秒数
	float followRate_ = 0.6f;               // 目標の速さに近づく強さ（1/秒。小さいほど、ふわっと遅れて揺れ、出たときの向きが長く残る）
	float prewarm_ = 0.5f;                  // 始まってすぐ出しておく数（Rate Over Time × Lifetime の Min に掛ける。0 なら出さず、だんだん増える）

	// ===== 実行中だけの値（保存しない）=====
	float swayTime_ = 0.0f;    // 揺れの時間（ポーズ・ヒットストップ中は進めない）
	int frameCount_ = 0;       // 動き始めてからのフレーム数（Prewarm を出すフレームを待つ）
	bool isPrewarmed_ = false; // Prewarm を出した
};
