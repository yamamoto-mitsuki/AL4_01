#include "Coin.h"

#include <format>

#include <MyEngine/Diagnostics/LogManager.h>

SCRIPT(Coin);

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void Coin::Fields(ComponentUI& ui) { ui.Field("Spin Speed", spinSpeed_, Tip("1秒に回す角度（度）")); }

//=============================================================================
// エンジンが呼ぶ関数
//=============================================================================
void Coin::Update() { Rotate(transform, {0.0f, 1.0f, 0.0f}, spinSpeed_ * Time::GetDeltaTime()); }

void Coin::OnDestroy() {
	// 消える直前。Transform はまだ読める（ここでパーティクルを出す・音を鳴らす、など）
	LogManager::Log(std::format("{} が消えた（x = {:.1f}）", gameObject.name, transform.translation.x));
}