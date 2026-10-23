#include "GoalDoor.h"

#include "IceCube.h"
#include "LevelInfo.h"

SCRIPT(GoalDoor, "Game");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void GoalDoor::Fields(ComponentUI& ui) { ui.Field("Next Level Delay", nextLevelDelay_, AtLeast(0.0f).Tip("キューブが触れてから、次のレベルへ進むまでの秒数（キューブが扉へ吸い込まれる間）")); }

//=============================================================================
// 当たり
//=============================================================================
void GoalDoor::OnTriggerEnter(GameObject other) {
	// --- 早期リターン：キューブでない・もうやられた・もうクリアした ---
	IceCube* cube = other.GetComponent<IceCube>();
	if (cube == nullptr || !cube->IsPlayable() || isCleared_) {
		return;
	}
	// --- クリア：キューブを扉へ吸い込ませて、時間を数え始める ---
	cube->Clear(GetWorldPosition(transform));
	isCleared_ = true;
}

//=============================================================================
// 毎フレーム
//=============================================================================
void GoalDoor::Update() {
	if (!isCleared_ || isNextRequested_) {
		return;
	}
	timer_ += Time::GetDeltaTime();
	if (timer_ < nextLevelDelay_) {
		return;
	}
	// --- 次のレベルへ（LevelInfo が無ければ、このレベルを最初から）---
	isNextRequested_ = true;
	if (const LevelInfo* level = LevelInfo::Find()) {
		level->LoadNextLevel();
	} else {
		SceneManager::ReloadScene();
	}
}