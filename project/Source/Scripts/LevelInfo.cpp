#include "LevelInfo.h"

#include <algorithm>
#include <format>
#include <vector>

#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/Input/InputManager.h>

#include "Source/Scripts/Audio/GameAudio.h"
#include "Source/Scripts/Gimmicks/SecretCrystal.h"
#include "Source/Scripts/Progress/CheckpointState.h"
#include "Source/Scripts/Progress/GameProgress.h"
#include "Source/Scripts/Progress/OutGameState.h"
#include "Source/Scripts/Progress/StageCatalog.h"
#include "Source/Scripts/UI/StageIntro.h"

SCRIPT(LevelInfo, "Game");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void LevelInfo::Fields(ComponentUI& ui) {
	ui.Label("R / パッドの Back：やり直す（中間地点を取っていたらそこから）。Esc / パッドの Start：ポーズ画面を出す（クリアした後はどちらも効かない）");
	ui.Field("Number", number_, AtLeast(1.0f).Tip("ステージの番号（StageCatalog.h の kStages の何番目か。1 から）。次のステージは一覧の次"));
	ui.Field("Area Name", areaName_, Tip("場所の名前"));
	ui.Field("Kill Height", killHeight_, Tip("これより下に落ちたら一発アウト（ワールドの Y）。足場の一番下より、少し下にする"));
	ui.Field("Pause Screen", pauseScreen_, Tip("Esc で出す画面の名前（resources/ui の .uiscreen の名前）。空なら、ポーズしない"));
	ui.Field("Intro Screen", introScreen_, Tip("ステージの始まりに出す画面の名前（ぼかし＋ステージの名前。StageIntro）。やられた・R の読み直しでは出さない。空なら出さない"));
	ui.Field("Title Scene", titleScene_, Tip("最後のステージの次に読むシーンの名前"));
}

//=============================================================================
// 最初・毎フレーム
//=============================================================================
void LevelInfo::Start() {
	// --- BGM：鳴っていなければ鳴らす。やられ・やり直しで下げていたら、元へゆっくり戻す（ここで「やり直した」と分かる）---
	GameAudio::StartBgm();
	// --- 中間地点：やられ・R で読み直したなら、時間は続きから。ほかの入り方（ポーズの Restart・ステージ選択・次のステージ）なら最初から ---
	// 中間地点までに取った隠し結晶も、取った扱いのまま持ち直す（結晶の側は SecretCrystal::Start が消えた姿にする）
	if (gCheckpoint.IsRespawning(GetStageIndex())) {
		playTime_ = gCheckpoint.playTime;
		collectedCrystals_ = gCheckpoint.crystals;
	} else {
		gCheckpoint.Reset();
	}
	// --- ステージの始まりの演出（ぼかし＋ステージの名前）：やられた・R の読み直しでは出さない ---
	const bool isRetry = gOutGame.isRetrying;
	gOutGame.isRetrying = false;
	if (!isRetry && !introScreen_.empty()) {
		UIScreen::Open(introScreen_);
	}
	// --- 番号が一覧に無ければ知らせる（記録も次のステージも無くなる）---
	if (GetStageIndex() >= kStageCount) {
		LogManager::Warning(std::format("LevelInfo：Number {} はステージの一覧（kStages。{} 個）に無いので、記録しません", number_, kStageCount));
		return;
	}
	gOutGame.lastStage = GetStageIndex(); // タイトルへ戻ったら、ステージ選択でこのステージを選ぶ
	CountCrystals();
}

//=============================================================================
// 隠し結晶
//=============================================================================
void LevelInfo::CountCrystals() const {
	// --- 置いてある結晶の Id を集める（同じ Id が2つあると、片方を取るともう片方も取った扱いになるので知らせる）---
	std::vector<int> ids;
	for (const SecretCrystal* crystal : FindObjectsOfType<SecretCrystal>()) {
		ids.push_back(crystal->GetId());
	}
	std::ranges::sort(ids);
	if (const auto duplicate = std::ranges::adjacent_find(ids); duplicate != ids.end()) {
		LogManager::Warning(std::format("LevelInfo：隠し結晶の Id {} が2つ以上あります（同じステージの結晶は Id を別々に）", *duplicate));
	}
	const auto [first, last] = std::ranges::unique(ids);
	ids.erase(first, last);
	// --- ステージ選択で「取った数 / 置いてある数」を出せるように、置いてある数を覚える ---
	GameProgress::SetCrystalTotal(GetStageIndex(), static_cast<int>(ids.size()));
}

void LevelInfo::KeepCrystals() const {
	// 取ってある中間地点がこのステージの物なら、今までに取った結晶を覚えてもらう（取った物は減らないので、上書きでよい）
	if (gCheckpoint.hasCheckpoint && gCheckpoint.stage == GetStageIndex()) {
		gCheckpoint.crystals = collectedCrystals_;
	}
}

void LevelInfo::Update() {
	// --- 中間地点から始める読み直しは、最初のフレームで終わり（全部の Start が見た後。次にほかの入り方をしたら最初から）---
	// Script はフレームの中で「全部の Start → 全部の Update」の順に呼ばれるので、ここで下ろしても、Start で見る物には間に合っている
	// 下ろすのは最初の Update だけ（毎フレーム下ろすと、Retry で立てた後、幕が閉じきるまでの間に消してしまう）
	if (!hasUpdated_) {
		hasUpdated_ = true;
		gCheckpoint.isRespawning = false;
	}
	// --- クリアした後は、時間を止めて、やり直し・ポーズも受けない ---
	if (isCleared_) {
		return;
	}
	// 始まりの演出の間（キューブがまだ出ていない）は数えない。ポーズ中はこの Update が呼ばれないので、数えない
	if (!IsIntroPlaying()) {
		playTime_ += Time::GetDeltaTime();
	}
	// --- やり直し：いつでも。中間地点を取っていたら、そこから（取っていなければレベルの最初から）---
	// 幕を閉じ始めたら（もう幕が動いていたら false）、BGM を低く・小さくする（幕が閉じきるまでに）
	if (InputManager::IsKeyTriggered(DIK_R) || InputManager::IsButtonTriggered(GamepadButton::Back)) {
		if (Retry(SceneTransition::kDefaultFadeTime)) {
			GameAudio::DownBgm(SceneTransition::kDefaultFadeTime);
		}
	}
	// --- ポーズ：画面を出す（閉じるのは、画面の中の PauseMenu。ポーズ中はこの Update が呼ばれないので）---
	if (!pauseScreen_.empty() && (InputManager::IsKeyTriggered(DIK_ESCAPE) || InputManager::IsButtonTriggered(GamepadButton::Start))) {
		if (UIScreen::Open(pauseScreen_)) {
			GameAudio::PlaySe(Se::UiOpen);
		}
	}
}

//=============================================================================
// 始まりの演出
//=============================================================================
bool LevelInfo::IsIntroPlaying() const {
	// 開いた直後は、画面の Script（StageIntro）の Start がまだなので、画面が出ているかも見る
	// 閉じ始めた後（UIScreen::IsOpen が false）は、ぼかしが戻りきるまで StageIntro::IsPlaying が true
	return (!introScreen_.empty() && UIScreen::IsOpen(introScreen_)) || StageIntro::IsPlaying();
}

//=============================================================================
// やり直し（中間地点から）
//=============================================================================
bool LevelInfo::Retry(float fadeTime) const {
	// 幕を閉じ始められなかった（もう幕が動いている：ポーズの Title・ゴールの後など）ら、何も覚えない
	// （覚えてしまうと、動いている幕の行き先が同じステージだったときに、中間地点から始まってしまう）
	if (!SceneTransition::ReloadScene(fadeTime)) {
		return false;
	}
	// 幕が閉じきって読み直すのは、このフレームより後なので、ここで覚えれば間に合う
	gOutGame.isRetrying = true; // 読み直した後は、ステージの始まりの演出を出さない（やられるたびに出ると、じゃまなので）
	if (gCheckpoint.hasCheckpoint && gCheckpoint.stage == GetStageIndex()) {
		gCheckpoint.isRespawning = true;
		gCheckpoint.playTime = playTime_; // 時間は、やられた所から続ける（中間地点を取った時に戻さない）
	}
	return true;
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
	GameProgress::RecordCrystals(index, collectedCrystals_); // 今回取った結晶は、ゴールしてはじめて記録する
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