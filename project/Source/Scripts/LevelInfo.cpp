#include "LevelInfo.h"

#include <MyEngine/Input/InputManager.h>

SCRIPT(LevelInfo, "Game");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void LevelInfo::Fields(ComponentUI& ui) {
	ui.Label("R / パッドの Back：レベルの最初からやり直す");
	ui.Field("Number", number_, AtLeast(1.0f).Tip("レベルの番号（G9 で左下に出す）"));
	ui.Field("Area Name", areaName_, Tip("場所の名前（森・海辺。G9 で左下に出す）"));
	ui.Field("Kill Height", killHeight_, Tip("これより下に落ちたら一発アウト（ワールドの Y）。足場の一番下より、少し下にする"));
	ui.Field("Next Scene", nextScene_, Tip("ゴールの後に読むシーンの名前（Level02 など。resources/scenes の .scene.json の名前）。空なら、このレベルをもう一度"));
}

//=============================================================================
// 毎フレーム
//=============================================================================
void LevelInfo::Update() {
	// --- やり直し：いつでもレベルの最初から ---
	if (InputManager::IsKeyTriggered(DIK_R) || InputManager::IsButtonTriggered(GamepadButton::Back)) {
		SceneManager::ReloadScene();
	}
}

//=============================================================================
// 次のレベル
//=============================================================================
void LevelInfo::LoadNextLevel() const {
	// 次のシーンが空・見つからない（名前の打ち間違い）なら、このレベルをもう一度（見つからないときは LoadScene が警告を出す）
	if (nextScene_.empty() || !SceneManager::LoadScene(nextScene_)) {
		SceneManager::ReloadScene();
		return;
	}
}