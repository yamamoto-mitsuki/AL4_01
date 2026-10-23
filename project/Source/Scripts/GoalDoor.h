#pragma once
#include <MyEngine/ECS/Script.h>

/// <summary>
/// ゴールの扉。Collider（Is Trigger）を付けた Entity に付ける
/// <para>氷のキューブ（IceCube）が触れたらクリア。キューブが扉へ吸い込まれている間（Next Level Delay 秒）待ってから、次のレベルへ（LevelInfo）</para>
/// </summary>
class GoalDoor : public Script {
public:
	void Update() override;
	void Fields(ComponentUI& ui) override;
	void OnTriggerEnter(GameObject other) override; // キューブが触れた

private:
	// ===== Inspector に出す・保存する値 =====
	float nextLevelDelay_ = 1.0f; // 触れてから、次のレベルへ進むまでの秒数

	// ===== 実行中だけの値（保存しない）=====
	bool isCleared_ = false;       // キューブが触れた
	float timer_ = 0.0f;           // 触れてからの秒数
	bool isNextRequested_ = false; // 次のレベルを頼んだ（何回も頼まないように）
};