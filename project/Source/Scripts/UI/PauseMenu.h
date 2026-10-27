#pragma once
#include <string>

#include "Source/Scripts/Effects/DepthOfFieldFade.h"
#include "Source/Scripts/UI/MenuBase.h"
#include "Source/Scripts/UI/TextHider.h"

/// <summary>
/// ポーズ画面。ポーズ画面（resources/ui/Pause.uiscreen）の一番上の Entity に付ける
/// <para>項目の名前で何をするか決める：Resume（つづける）/ Restart（やりなおす）/ Settings（設定の画面を重ねる）/ Title（タイトルへ）。選ぶ・飾りは MenuBase</para>
/// <para>設定の画面（Settings Screen。SettingsMenu の付いた uiscreen）が出ている間は、ポーズの項目は動かず、
/// 見出し（Header）と項目（Items）が静かに消える（Alpha Group）。閉じたら戻る</para>
/// <para>開いている間は、ゲームの絵をぼかす（シーンの Depth Of Field を動かす。閉じ始めたら戻す。Restart・Title はシーンを読み直すので戻さなくてよい）。
/// ぼかしは Text に掛からないので、ゲームの中の Text（操作の説明など）は、ぼかしと一緒に消して・戻す（TextHider）</para>
/// <para>ポーズ中（ゲームが止まっている間）も、開いている画面の中の Script なので呼ばれる</para>
/// </summary>
class PauseMenu : public MenuBase {
public:
	void Fields(ComponentUI& ui) override;

protected:
	void OnStart() override;  // コントローラーの振動を止める・ぼかし始める
	void OnUpdate() override; // ぼかしを進める・設定の画面が出ている間、見出しと項目を消す・閉じたら戻す
	void OnDestroy() override; // 画面が消えた：ぼかしを戻しきる（戻している途中で消えても、ぼけたまま残らないように）
	void Decide(int index, const std::string& itemName) override;
	void OnCancel() override;             // 戻るの音を鳴らして、閉じる
	bool IsInputBlocked() const override; // 設定の画面が出ている間（と、閉じた次のフレーム）

private:
	// 閉じて、ゲームに戻る（Resume を決めたときは、決めた音だけにしたいので、戻るの音は鳴らさない）
	void Close();
	// 子の name の Alpha Group の濃さを alpha にする（無ければ何もしない。Alpha Group は UIScreen が直下の子に付ける）
	void SetChildAlpha(const std::string& name, float alpha) const;

	// ===== Inspector に出す・保存する値 =====
	std::string screenName_ = "Pause";                 // この画面の名前（閉じるときに UIScreen::Close に渡す）
	std::string titleScene_ = "Title";                 // タイトルのシーンの名前（resources/scenes/<名前>.scene.json）
	std::string settingsScreen_ = "PauseSettings";     // Settings で重ねる画面の名前（resources/ui/<名前>.uiscreen）
	std::string headerName_ = "Header";                // 設定の画面を出している間に消す見出しの Text（一番上の直下の子。項目の Items も消す）
	float fadeTime_ = 0.2f;                            // 設定の画面を出した・閉じたとき、見出しと項目が消える・戻るまでの秒数
	float blurTime_ = 0.25f;                           // 開いた・閉じたとき、ぼける・戻るまでの秒数
	DepthOfFieldFade::Settings blurTo_ = {             // ぼけている時の値（カメラの 1m 先より奥が全部ぼける）
	    .focusDistance = 1.0f,
	    .focusRange = 0.0f,
	    .farTransition = 3.0f,
	    .nearTransition = 0.0f,
	    .maxBlur = 16.0f,
	};

	// ===== 実行中だけの値 =====
	bool isSubBlocking_ = false; // 入力を受けない（設定の画面が出ている・閉じた次のフレーム）
	bool wasSubOpen_ = false;    // 前のフレームに設定の画面が出ていた
	float contentAlpha_ = 1.0f;  // 見出しと項目の濃さ（設定の画面を出している間は 0 へ）
	DepthOfFieldFade blur_;      // ぼかしの動き
	TextHider gameTexts_;        // ゲームの中の Text（ぼかしている間は消す）
	float gameTextAlpha_ = 1.0f; // ゲームの中の Text の濃さ（開いたら 0 へ・閉じ始めたら 1 へ。Blur Time 秒で）
	bool isClosing_ = false;     // 閉じ始めた（ぼかしと Text を戻している）
};
