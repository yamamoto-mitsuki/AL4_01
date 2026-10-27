#include "GameRumble.h"

#include <algorithm>

#include <MyEngine/Input/InputManager.h>

namespace {
float remainingTime = 0.0f; // 震えを止めるまでの残りの秒数
bool isRumbling = false;    // 震えている（止めていない）
} // namespace

//=============================================================================
// 震わせる・止める
//=============================================================================
void GameRumble::Play(float power, float duration) {
	if (duration <= 0.0f) {
		return;
	}
	// 左（低い・ドスン）と右（高い・ブルブル）のモーターを同じ強さで。強さは途中で変えない（一定）
	const float clamped = std::clamp(power, 0.0f, 1.0f);
	InputManager::SetVibration(clamped, clamped);
	remainingTime = duration;
	isRumbling = true;
}

void GameRumble::Update(float deltaTime) {
	if (!isRumbling) {
		return;
	}
	remainingTime -= deltaTime;
	if (remainingTime <= 0.0f) {
		Stop();
	}
}

void GameRumble::Stop() {
	if (!isRumbling) {
		return; // 震えていない（毎回 XInput に「止めて」を送らない）
	}
	isRumbling = false;
	remainingTime = 0.0f;
	InputManager::StopVibration();
}
