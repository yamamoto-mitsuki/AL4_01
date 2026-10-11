#include "PlayerController.h"

#include <format>

#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/Input/InputManager.h>

#include "Coin.h"

SCRIPT(PlayerController);

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void PlayerController::Fields(ComponentUI& ui) { ui.Field("Move Speed", moveSpeed_, AtLeast(0.0f).Tip("1秒に進む距離")); }

//=============================================================================
// エンジンが呼ぶ関数
//=============================================================================
void PlayerController::Start() {
	// シーンにあるコインを数える（有効な GameObject に付いている物だけ）
	const size_t coins = FindObjectsOfType<Coin>().size();
	LogManager::Log(std::format("{} がスタート。コインは {} 枚", gameObject.name, coins));
}

void PlayerController::Update() {
	// WASD（パッドは使わない）。上下の入力を奥・手前に使う
	const Vector2 move = InputManager::GetMoveAxis(false);
	transform.translation += Vector3{move.x, 0.0f, move.y} * moveSpeed_ * Time::GetDeltaTime();
}

void PlayerController::OnTriggerEnter(GameObject other) {
	// 相手がコインか（相手が何かは「持っている Script・Component」で見分ける。エンジンにタグは無い）
	if (other.GetComponent<Coin>() == nullptr) {
		return;
	}
	++coinCount_;
	Destroy(other); // 消えるのはフレームの終わり
	LogManager::Log(std::format("{} を拾った（{} 枚目）", other.name, coinCount_));
}