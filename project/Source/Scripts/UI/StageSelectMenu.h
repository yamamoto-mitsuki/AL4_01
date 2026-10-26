#pragma once
#include <string>
#include <vector>

#include <MyEngine/Math/Vector2.h>
#include <MyEngine/Math/Vector4.h>

#include "Source/Scripts/UI/MenuBase.h"

/// <summary>
/// ステージ選択の画面。resources/ui/StageSelect.uiscreen の一番上の Entity に付ける（タイトルの上に重ねる。シーンは替えない）
/// <para>マスは、ステージの一覧（StageCatalog.h の kStages）の数だけ、見本の Text（Cell Template）を写して子の Items の下に作る。
/// 一覧に行を足せば、マスも増える</para>
/// <para>マスの後ろに、タイトルへ戻る項目（一番上の直下の子の Back。Text）を1つ足す。マスの一番下の段で ↓ を押すと Back へ行く</para>
/// <para>下の2行（Stage Name・Best Time の Text）に、選んでいるステージの名前と1位のタイムを出す</para>
/// <para>遊べるのは、1つ前をクリアしたステージだけ。決めたら、そのシーンへ（幕）。Back か戻るキーで閉じる（タイトルへ戻る）</para>
/// </summary>
class StageSelectMenu : public MenuBase {
public:
	void Fields(ComponentUI& ui) override;

protected:
	void OnStart() override;  // マスを作って、最初に選ぶマスを決める
	void OnUpdate() override; // 入れないマスを決めたときの言葉を、しばらくしたら戻す
	void Decide(int index, const std::string& itemName) override;
	void OnCancel() override;
	bool CanDecide(int index) const override;
	void OnDecideRejected(int index) override;
	void OnSelect(int index) override;
	std::vector<Handle<Entity>> CollectItems() const override; // マス（Items の子）の後ろに Back を足す

private:
	// マスを作る（見本の Text を写して、文字・色・位置だけ変える）
	void CreateCells();
	// 子の name の Text に文字を書く（無ければ何もしない）
	void SetText(const std::string& name, const std::string& text) const;
	// index の項目が Back か（マスは一覧の数だけあるので、それより後ろは Back）
	static bool IsBackIndex(int index);

	// ===== Inspector に出す・保存する値 =====
	std::string screenName_ = "StageSelect";              // この画面の名前（閉じるときに UIScreen::Close に渡す）
	std::string templateName_ = "CellTemplate";           // マスの見本の Text（一番上の直下の子。Text の Enabled を外して置く。この位置が1つ目のマス）
	std::string backName_ = "Back";                       // タイトルへ戻る項目の Text（一番上の直下の子。無ければ戻るキーだけ）
	Vector2 cellSpacing_ = {120.0f, 96.0f};               // マスとマスの間（ピクセル。横・縦。2段目は下へ）
	Vector4 clearedColor_ = {1.0f, 1.0f, 1.0f, 1.0f};     // クリアしたマスの色
	Vector4 openColor_ = {1.0f, 1.0f, 1.0f, 0.6f};        // 遊べるがまだクリアしていないマスの色
	Vector4 lockedColor_ = {1.0f, 1.0f, 1.0f, 0.18f};     // まだ遊べないマスの色
	std::string stageNameName_ = "StageName";             // ステージの名前を書く Text（一番上の直下の子）
	std::string bestTimeName_ = "BestTime";               // 1位のタイムを書く Text（一番上の直下の子）
	std::string bestLabel_ = "Best";                      // タイムの前に付ける言葉
	std::string noRecordText_ = "--:--.--";               // まだクリアしていないときのタイム
	std::string lockedText_ = "Locked";                   // まだ遊べないときに、タイムの代わりに出す言葉
	std::string comingText_ = "Coming soon";              // シーンがまだ無いときに、タイムの代わりに出す言葉
	std::string lockedHint_ = "Clear the previous stage"; // まだ遊べないマスを決めたときに、しばらく出す言葉
	float hintTime_ = 1.5f;                               // 上の言葉を出しておく秒数

	// ===== 実行中だけの値 =====
	float hintTimer_ = 0.0f; // Locked Hint を出しておく残り秒（0 なら出していない）
};