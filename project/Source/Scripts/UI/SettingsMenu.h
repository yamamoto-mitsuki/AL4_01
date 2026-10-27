#pragma once
#include <string>

#include "Source/Scripts/UI/MenuBase.h"
#include "Source/Scripts/UI/SettingItems.h"

/// <summary>
/// 設定の画面。resources/ui/Settings.uiscreen（タイトルの上に重ねる）・PauseSettings.uiscreen（ポーズ画面の上に重ねる。Reset は置かない）の一番上の Entity に付ける（シーンは替えない）
/// <para>項目の名前で何をするか決める（選ぶ・飾りは MenuBase）：</para>
/// <para>Master・SE・BGM（全体・効果音・音楽の大きさ。←→ で 0〜10 の段。決めると1段上げ、一番上の次は 0）/ Fullscreen（←→・決めるで切り替え）/
/// Reset（記録を消す。1回目は確かめる言葉を赤い Text に出し、もう一度決めたら消して「消去中です...」を出す）/ Back（閉じてタイトルへ）</para>
/// <para>音の大きさ・全画面の動き（値を Value の Text に書く・すぐ当てて保存する）は SettingItems</para>
/// </summary>
class SettingsMenu : public MenuBase {
public:
	void Fields(ComponentUI& ui) override;

protected:
	void OnStart() override;  // 値を書く
	void OnUpdate() override; // Value の飾りを項目に合わせる・Reset の言葉を戻す
	void Decide(int index, const std::string& itemName) override;
	void OnCancel() override;
	void OnSelect(int index) override;               // Reset から離れたら、確かめるのをやめる
	void OnAdjust(int index, int direction) override; // ←→ で値を変える
	bool IsInputBlocked() const override;           // 「消去中です...」を出している間は、操作を受けない（消し終わって言葉が消えたら受ける）

private:
	// 全部の項目の値（Value の Text）と、Reset の言葉・確かめる言葉を書く
	void RefreshTexts() const;
	// 子の Items の下の項目 name の Text に文字を書く（無ければ何もしない）
	void SetItemText(const std::string& name, const std::string& text) const;
	// 閉じて、下の画面（タイトル・ポーズ）へ戻る（Back を決めたときは、決めた音だけにしたいので、戻るの音は鳴らさない）
	void Close();

	// ===== Inspector に出す・保存する値 =====
	std::string screenName_ = "Settings";               // この画面の名前（閉じるときに UIScreen::Close に渡す）
	SettingItems::ValueTexts valueTexts_;               // 値を書く Text の名前（項目の直下の子）と、全画面の On・Off の言葉
	std::string resetText_ = "Reset Data";              // Reset の項目の言葉
	std::string resetWarningName_ = "ResetWarning";     // Reset を1回決めたときに、確かめる言葉を書く Text の名前（一番上の直下の子。色は Text の方で決める）
	std::string resetWarningText_ = "データを消去します。本当にいいですか？"; // 確かめる言葉（もう一度決めたら消す）
	std::string resetErasingText_ = "消去中です";        // 2回目を決めたときに、確かめる言葉の Text に出す言葉（後ろに「.」「..」「...」が付いて動く）
	float resetErasingTime_ = 1.8f;                     // 上の言葉を出しておく秒数
	float dotInterval_ = 0.3f;                          // 「.」が1つ増えるまでの秒数（「...」の次は「.」に戻る）

	// ===== 実行中だけの値 =====
	bool isResetArmed_ = false;   // Reset を1回決めた（もう一度決めたら消す）
	float resetErasingTimer_ = 0.0f; // Reset Erasing Text を出しておく残り秒（0 なら出していない）
};
