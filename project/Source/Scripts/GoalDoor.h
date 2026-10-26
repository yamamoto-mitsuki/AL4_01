#pragma once
#include <string>

#include <MyEngine/ECS/Script.h>

#include "Source/Scripts/Effects/DepthOfFieldFade.h"

/// <summary>
/// ゴールの扉。Collider（Is Trigger）を付けた Entity に付ける
/// <para>氷のキューブ（IceCube）が触れたらクリア：キューブが扉へ吸い込まれ、子の Clear Particle（Particle System）の粒がふわふわ昇り始め、
/// ゲームの絵がゆっくりぼけていく（被写界深度）。Screen Delay 秒たったら、クリア画面（UI Screen）を出す。シーンは替えない</para>
/// <para>クリア画面が無いとき（ファイルが無い）は、次のレベルへ（LevelInfo）</para>
/// </summary>
class GoalDoor : public Script {
public:
	void Update() override;
	void Fields(ComponentUI& ui) override;
	void OnTriggerEnter(GameObject other) override; // キューブが触れた

private:
	// ===== Inspector に出す・保存する値 =====
	// --- クリア画面 ---
	std::string clearScreen_ = "Clear"; // 出す画面の名前（resources/ui/<名前>.uiscreen）
	float screenDelay_ = 1.0f;          // 触れてから、クリア画面を出すまでの秒数（キューブが扉へ吸い込まれる間）
	// --- 昇る粒 ---
	std::string particleName_ = "ClearParticle"; // 触れたときに流し始める Particle System の名前（この Entity の直下の子。Play On Start は外して置く）
	// --- ぼかし（被写界深度。触れた瞬間から、今の値をこの値へ動かす）---
	float blurTime_ = 1.4f;             // ぼけきるまでの秒数
	DepthOfFieldFade::Settings blurTo_; // ぼけきった時の値（初めは、カメラの 2m 先より奥が全部ぼける）

	// ===== 実行中だけの値（保存しない）=====
	bool isCleared_ = false;         // キューブが触れた
	float timer_ = 0.0f;             // 触れてからの秒数
	bool isScreenRequested_ = false; // クリア画面を頼んだ（何回も頼まないように）
	DepthOfFieldFade blur_;          // ぼかしの動き
};