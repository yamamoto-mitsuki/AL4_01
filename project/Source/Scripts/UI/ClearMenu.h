#pragma once
#include <string>

#include <MyEngine/Math/Vector4.h>

#include "Source/Scripts/UI/MenuBase.h"

/// <summary>
/// クリア画面。resources/ui/Clear.uiscreen の一番上の Entity に付ける（ゴールの扉 GoalDoor が出す。シーンは替えない・ゲームも止めない）
/// <para>開いたとき、子の Text に書く：Stage Text（ステージの名前）・Time Text（遊んだ時間）・Record Text（New Best か、前の1位のタイム）</para>
/// <para>項目の名前で何をするか決める：Next（次のステージへ。次が無ければ、項目ごと消す）/ Title（タイトルへ）</para>
/// <para>文字がゆっくり出てくる間（Input Delay 秒）は、入力を受けない（見えないうちに決めてしまわないように）</para>
/// </summary>
class ClearMenu : public MenuBase {
public:
	void Fields(ComponentUI& ui) override;

protected:
	void OnStart() override;  // 文字を書く・次が無ければ Next を消す
	void OnUpdate() override; // 開いてからの秒を数える
	void Decide(int index, const std::string& itemName) override;
	bool IsInputBlocked() const override; // 開いてから Input Delay 秒の間

private:
	// 子の name の Text を返す（無ければ nullptr。ポインタは使い捨て）
	TextComponent* FindText(const std::string& name) const;

	// ===== Inspector に出す・保存する値 =====
	// --- 書く Text（一番上の直下の子の名前）---
	std::string stageTextName_ = "StageText";   // ステージの名前
	std::string timeTextName_ = "TimeText";     // 遊んだ時間
	std::string recordTextName_ = "RecordText"; // New Best か、前の1位のタイム
	// --- 言葉 ---
	std::string newBestText_ = "New Best"; // 1位になった（初めてのクリアも）
	std::string bestLabel_ = "Best";       // 1位でなかったときに、前の1位のタイムの前に付ける言葉
	// --- New Best の見た目（1位でなかったときは、Record Text の見た目のまま）---
	Vector4 newBestColor_ = {0.85f, 0.95f, 1.0f, 1.0f};      // 文字の色
	float newBestGlowWidth_ = 8.0f;                          // うっすら光る幅
	Vector4 newBestGlowColor_ = {0.75f, 0.93f, 1.0f, 0.55f}; // 光の色（氷色）
	// --- 入力 ---
	float inputDelay_ = 1.0f;           // 開いてから、入力を受けるまでの秒数（項目が見えてくるまで）
	std::string nextItemName_ = "Next"; // 次のステージへの項目の名前（次が無ければ消す）

	// ===== 実行中だけの値 =====
	float elapsed_ = 0.0f; // 開いてからの秒数（止めない時間で数える）
};