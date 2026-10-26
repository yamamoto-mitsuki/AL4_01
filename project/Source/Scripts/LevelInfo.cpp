#include "LevelInfo.h"

#include <format>

#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/Input/InputManager.h>

#include "Source/Scripts/Progress/GameProgress.h"
#include "Source/Scripts/Progress/OutGameState.h"
#include "Source/Scripts/Progress/StageCatalog.h"

SCRIPT(LevelInfo, "Game");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void LevelInfo::Fields(ComponentUI& ui) {
	ui.Label("R / パッドの Back：レベルの最初からやり直す。Esc / パッドの Start：ポーズ画面を出す（クリアした後はどちらも効かない）");
	ui.Field("Number", number_, AtLeast(1.0f).Tip("ステージの番号（StageCatalog.h の kStages の何番目か。1 から）。次のステージは一覧の次"));
	ui.Field("Area Name", areaName_, Tip("場所の名前"));
	ui.Field("Kill Height", killHeight_, Tip("これより下に落ちたら一発アウト（ワールドの Y）。足場の一番下より、少し下にする"));
	ui.Field("Pause Screen", pauseScreen_, Tip("Esc で出す画面の名前（resources/ui の .uiscreen の名前）。空なら、ポーズしない"));
	ui.Field("Title Scene", titleScene_, Tip("最後のステージの次に読むシーンの名前"));
}

//=============================================================================
// 最初・毎フレーム
//=============================================================================
void LevelInfo::Start() {
	// --- 番号が一覧に無ければ知らせる（記録も次のステージも無くなる）---
	if (GetStageIndex() >= kStageCount) {
		LogManager::Warning(std::format("LevelInfo：Number {} はステージの一覧（kStages。{} 個）に無いので、記録しません", number_, kStageCount));
		return;
	}
	gOutGame.lastStage = GetStageIndex(); // タイトルへ戻ったら、ステージ選択でこのステージを選ぶ
}

void LevelInfo::Update() {
	// --- クリアした後は、時間を止めて、やり直し・ポーズも受けない ---
	if (isCleared_) {
		return;
	}
	playTime_ += Time::GetDeltaTime(); // ポーズ中はこの Update が呼ばれないので、数えない
	// --- やり直し：いつでもレベルの最初から ---
	if (InputManager::IsKeyTriggered(DIK_R) || InputManager::IsButtonTriggered(GamepadButton::Back)) {
		SceneManager::ReloadScene();
	}
	// --- ポーズ：画面を出す（閉じるのは、画面の中の PauseMenu。ポーズ中はこの Update が呼ばれないので）---
	if (!pauseScreen_.empty() && (InputManager::IsKeyTriggered(DIK_ESCAPE) || InputManager::IsButtonTriggered(GamepadButton::Start))) {
		UIScreen::Open(pauseScreen_);
	}
}

//=============================================================================
// クリア
//=============================================================================
void LevelInfo::Clear() {
	if (isCleared_) {
		return;
	}
	isCleared_ = true;
	const size_t index = GetStageIndex();
	if (index >= kStageCount) {
		return; // 一覧に無いステージ（Start で知らせた）
	}
	previousBest_ = GameProgress::RecordClear(index, playTime_);
	gOutGame.lastStage = HasNextLevel() ? index + 1 : index; // タイトルへ戻ったら、次のステージを選ぶ
}

//=============================================================================
// 次のレベル
//=============================================================================
bool LevelInfo::HasNextLevel() const {
	const size_t next = GetStageIndex() + 1;
	return next < kStageCount && SceneManager::HasScene(std::string(kStages[next].scene));
}

bool LevelInfo::LoadNextLevel() const {
	// 次が無ければ（最後のステージ・まだ作っていない）タイトルへ
	const std::string scene = HasNextLevel() ? std::string(kStages[GetStageIndex() + 1].scene) : titleScene_;
	return SceneTransition::LoadScene(scene);
}

bool LevelInfo::LoadTitle() const { return SceneTransition::LoadScene(titleScene_); }