#pragma once
#include <string>

#include <MyEngine/ECS/Script.h>

#include "Source/Scripts/Effects/DepthOfFieldFade.h"
#include "Source/Scripts/UI/TextHider.h"

/// <summary>
/// ステージの始まりの演出。resources/ui/StageIntro.uiscreen の一番上の Entity に付ける（LevelInfo::Start が開く。やられた・R の読み直しでは開かない）
/// <para>流れ：開いたらすぐ、ゲームの絵をぼかして、子の Stage Text にステージの名前（StageCatalog.h の title）を書き、ほかの Text（操作の説明など）を見えなくする
/// → 幕が開ききってから Hold Time 秒たったら、ぼかしを Clear Time 秒で元へ戻し、画面を閉じる（文字は UI Screen の消し方でフェード）。ほかの Text は、ぼかしと一緒に戻る</para>
/// <para>ぼかしが戻りきるまで IsPlaying が true。スタート地点の中間地点（Checkpoint の Is Start）は、それを待ってからキューブを出す</para>
/// </summary>
class StageIntro : public Script {
public:
	void Start() override;
	void Update() override;
	void OnDestroy() override; // ぼかしを戻しきる前に消えた（閉じる動きが短い・シーンが替わった）ら、すぐ元の値にする
	void Fields(ComponentUI& ui) override;

	// 演出の途中か（開いてから、ぼかしが戻りきるまで）。画面を開いた直後（この Script の Start の前）は false なので、UIScreen::IsOpen と一緒に見る
	static bool IsPlaying();

private:
	// 終わり：ぼかしを戻しきり、Text を元に戻す（何回呼んでもよい）
	void Finish();

	// ===== Inspector に出す・保存する値 =====
	std::string screenName_ = "StageIntro";    // この画面の名前（閉じるときに UIScreen::Close に渡す）
	std::string stageTextName_ = "StageText";  // ステージの名前を書く Text の名前（一番上の直下の子）
	float holdTime_ = 0.6f;                    // 幕が開ききってから、消し始めるまでの秒数
	float clearTime_ = 0.9f;                   // ぼかしが元へ戻るまでの秒数（文字は UI Screen の Hide Duration。こちらより少し長くする）
	DepthOfFieldFade::Settings blurTo_ = {     // ぼけている時の値（初めは、カメラの 1m 先より奥が全部ぼける）
	    .focusDistance = 1.0f,
	    .focusRange = 0.0f,
	    .farTransition = 3.0f,
	    .nearTransition = 0.0f,
	    .maxBlur = 24.0f,
	};

	// ===== 実行中だけの値（保存しない）=====
	float timer_ = 0.0f;                  // 幕が開ききってからの秒数
	float clearTimer_ = 0.0f;             // 消し始めてからの秒数
	bool isClosing_ = false;              // 消し始めた
	bool isFinished_ = false;             // 終わった（ぼかし・Text を戻しきった）
	DepthOfFieldFade blur_;               // ぼかしの動き
	TextHider hiddenTexts_;               // 見えなくした、ほかの Text（ぼかしは Text に掛からないので）
};
