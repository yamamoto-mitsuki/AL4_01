#include "StageIntro.h"

#include <algorithm>
#include <format>

#include <MyEngine/Components/UI/TextComponent.h>
#include <MyEngine/Diagnostics/LogManager.h>

#include "Source/Scripts/LevelInfo.h"
#include "Source/Scripts/Progress/StageCatalog.h"

SCRIPT(StageIntro, "UI");

namespace {
bool isPlaying = false;            // 演出の途中（StageIntro::IsPlaying。シーンに演出は1つだけなので、ここに1つ）
constexpr float kTextFadeRate = 0.3f; // 隠した Text を戻すのは、ぼかしを戻す時間の終わりのこの割合の間（ぼけている間は見せない）
} // namespace

bool StageIntro::IsPlaying() { return isPlaying; }

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void StageIntro::Fields(ComponentUI& ui) {
	ui.Label("開いたらすぐ、ゲームの絵をぼかして Stage Text にステージの名前を書き、ほかの Text を隠す → 幕が開ききって Hold Time 秒後に、ぼかしを戻して閉じる");
	ui.Field("Screen Name", screenName_, Tip("この画面の名前（resources/ui/<名前>.uiscreen）。閉じるときに使う"));
	ui.Field("Stage Text", stageTextName_, Tip("ステージの名前（StageCatalog.h の title）を書く Text の名前（一番上の直下の子）"));
	ui.Field("Hold Time", holdTime_, AtLeast(0.0f).Tip("幕が開ききってから、ぼかしと文字を消し始めるまでの秒数"));
	ui.Field("Clear Time", clearTime_, AtLeast(0.01f).Tip("ぼかしが元へ戻るまでの秒数。文字が消える秒数（UI Screen の Hide Duration）は、これより少し長くする"));
	ui.Field("Blur Focus Distance", blurTo_.focusDistance, AtLeast(0.0f).Tip("ぼけている時のピントの奥行き（m）。小さいほど、遊ぶ面まで全部ぼける"));
	ui.Field("Blur Focus Range", blurTo_.focusRange, AtLeast(0.0f).Tip("ぼけている時の、ぼけない幅（m）"));
	ui.Field("Blur Far Transition", blurTo_.farTransition, AtLeast(0.0f).Tip("ぼけている時の、一番ぼけるまでの奥行き（m）。小さいほど、すぐ奥が強くぼける"));
	ui.Field("Blur Near Transition", blurTo_.nearTransition, AtLeast(0.0f).Tip("ぼけている時の、手前のぼけ（m）。0 なら手前はぼかさない"));
	ui.Field("Blur Max", blurTo_.maxBlur, AtLeast(0.0f).Tip("ぼけている時の、ぼけの半径（ピクセル。エンジンの上限は 32）"));
}

//=============================================================================
// 最初
//=============================================================================
void StageIntro::Start() {
	isPlaying = true;
	// --- ステージの名前（LevelInfo が無い・一覧に無い番号なら、見本の文字のまま）---
	const LevelInfo* level = LevelInfo::Find();
	if (level != nullptr && level->GetStageIndex() < kStageCount) {
		if (const GameObject text = gameObject.FindChild(stageTextName_)) {
			if (TextComponent* component = text.GetComponent<TextComponent>()) {
				component->text = std::string(kStages[level->GetStageIndex()].title);
			}
		}
	}
	// --- ほかの Text（シーンに置いた操作の説明など。開いている画面の中の物は除く）を隠す：透明度を覚えて 0 に ---
	hiddenTexts_.Capture();
	hiddenTexts_.SetAlpha(0.0f);
	// --- すぐぼかす（幕が開き始めた時には、もうぼけている）---
	// Begin は「今の値から動かす」ので、とても短い秒数で始めて、すぐ終わりまで進める
	if (blur_.Begin(blurTo_, 0.0f)) {
		blur_.Finish();
	} else {
		LogManager::Warning("StageIntro：シーンに Depth Of Field が無いので、ぼかしません（文字だけ出します）");
	}
}

//=============================================================================
// 毎フレーム
//=============================================================================
void StageIntro::Update() {
	// --- 幕が開ききるまで待つ（ぼけたまま・文字も出したまま）---
	if (isFinished_ || SceneTransition::IsBusy()) {
		return;
	}
	const float deltaTime = Time::GetDeltaTime(); // ポーズ中は進めない（ポーズを閉じたら続きから）
	if (!isClosing_) {
		// --- Hold Time 秒たったら、ぼかしを戻し始めて、画面を閉じる（文字は UI Screen の消し方でフェード）---
		timer_ += deltaTime;
		if (timer_ >= holdTime_) {
			isClosing_ = true;
			blur_.Restore(clearTime_);
			UIScreen::Close(screenName_);
		}
		return;
	}
	// --- 消している途中：ぼかしを戻す。隠した Text は、ぼかしがほとんど消えてから（ぼかしを戻す時間の、終わりの kTextFadeRate の割合の間に）戻す。戻しきったら終わり ---
	blur_.Update(deltaTime);
	clearTimer_ += deltaTime;
	const float t = std::clamp(clearTimer_ / clearTime_, 0.0f, 1.0f);
	hiddenTexts_.SetAlpha(std::clamp((t - (1.0f - kTextFadeRate)) / kTextFadeRate, 0.0f, 1.0f));
	if (clearTimer_ >= clearTime_) {
		Finish();
	}
}

void StageIntro::OnDestroy() { Finish(); }

//=============================================================================
// 終わり
//=============================================================================
void StageIntro::Finish() {
	if (isFinished_) {
		return;
	}
	isFinished_ = true;
	isPlaying = false;
	blur_.Finish();
	hiddenTexts_.SetAlpha(1.0f);
}
