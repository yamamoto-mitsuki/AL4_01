#include "ClearMenu.h"

#include <format>
#include <optional>

#include "Source/Scripts/LevelInfo.h"
#include "Source/Scripts/Progress/GameProgress.h"
#include "Source/Scripts/Progress/StageCatalog.h"

SCRIPT(ClearMenu, "UI");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void ClearMenu::Fields(ComponentUI& ui) {
	MenuBase::Fields(ui);
	ui.Label("項目の名前で何をするか決める：Next（次のステージへ。次が無ければ消す）/ Title（タイトルへ）");
	// --- 書く Text ---
	ui.Field("Stage Text", stageTextName_, Tip("ステージの名前（StageCatalog.h の title）を書く Text の名前（一番上の直下の子）"));
	ui.Field("Time Text", timeTextName_, Tip("遊んだ時間を書く Text の名前（一番上の直下の子）"));
	ui.Field("Record Text", recordTextName_, Tip("New Best か、前の1位のタイムを書く Text の名前（一番上の直下の子）"));
	ui.Field("Crystals", crystalsName_, Tip("隠し結晶の図形と数を入れた物の名前（一番上の直下の子）。結晶の無いステージでは消す"));
	ui.Field("Crystal Count Text", crystalCountName_, Tip("隠し結晶の「取った数 / 置いてある数」を書く Text の名前（Crystals の子）"));
	// --- 言葉 ---
	ui.Field("New Best Text", newBestText_, Tip("1位になったとき（初めてのクリアも）に出す言葉"));
	ui.Field("Best Label", bestLabel_, Tip("1位でなかったときに、前の1位のタイムの前に付ける言葉"));
	// --- New Best の見た目 ---
	ui.Field("New Best Color", newBestColor_, Tip("New Best の文字の色（1位でなかったときは、Record Text の色のまま）"));
	ui.Field("New Best Glow Width", newBestGlowWidth_, AtLeast(0.0f).Tip("New Best がうっすら光る幅"));
	ui.Field("New Best Glow Color", newBestGlowColor_, Tip("New Best の光の色"));
	// --- 入力 ---
	ui.Field("Input Delay", inputDelay_, AtLeast(0.0f).Tip("開いてから、入力を受けるまでの秒数。項目（Next・Title）が見えてくる時間に合わせる"));
	ui.Field("Next Item Name", nextItemName_, Tip("次のステージへの項目の名前（Items の子）。最後のステージなど、次が無ければ消す"));
}

//=============================================================================
// 最初・毎フレーム
//=============================================================================
void ClearMenu::OnStart() {
	const LevelInfo* level = LevelInfo::Find();
	if (level == nullptr) {
		return; // エディタで画面だけ開いたとき（文字は見本のまま）
	}
	// --- ステージの名前・遊んだ時間 ---
	const size_t stage = level->GetStageIndex();
	if (TextComponent* text = FindText(stageTextName_); text != nullptr && stage < kStageCount) {
		text->text = std::string(kStages[stage].title);
	}
	if (TextComponent* text = FindText(timeTextName_)) {
		text->text = FormatTime(level->GetPlayTime());
	}
	// --- 隠し結晶：取った数 / 置いてある数（クリアで記録した後の数）。結晶の無いステージでは消す ---
	if (const GameObject crystals = gameObject.FindChild(crystalsName_)) {
		const int total = (stage < kStageCount) ? GameProgress::GetCrystalTotal(stage) : 0;
		if (total <= 0) {
			crystals.SetActive(false);
		} else if (const GameObject count = crystals.FindChild(crystalCountName_)) {
			if (TextComponent* text = count.GetComponent<TextComponent>()) {
				text->text = std::format("{} / {}", GameProgress::GetCrystalCount(stage), total);
			}
		}
	}
	// --- 記録：1位になったら New Best（うっすら光る）。ならなかったら、前の1位のタイムを薄く ---
	if (TextComponent* text = FindText(recordTextName_)) {
		const std::optional<float> previousBest = level->GetPreviousBest();
		if (level->IsNewBest()) {
			text->text = newBestText_;
			text->color = newBestColor_;
			text->glowWidth = newBestGlowWidth_;
			text->glowColor = newBestGlowColor_;
		} else if (previousBest) {
			text->text = std::format("{}  {}", bestLabel_, FormatTime(*previousBest));
		}
	}
	// --- 次のステージが無ければ、Next を消す（消した項目は選べない。Title が1つ目になる）---
	if (!level->HasNextLevel()) {
		if (const GameObject items = gameObject.FindChild("Items")) {
			if (const GameObject next = items.FindChild(nextItemName_)) {
				next.SetActive(false);
			}
		}
	}
	Select(0);
}

void ClearMenu::OnUpdate() { elapsed_ += Time::GetUnscaleDeltaTime(); }

bool ClearMenu::IsInputBlocked() const { return elapsed_ < inputDelay_; }

//=============================================================================
// 決める
//=============================================================================
void ClearMenu::Decide(int /*index*/, const std::string& itemName) {
	const LevelInfo* level = LevelInfo::Find();
	if (itemName == nextItemName_) {
		// 次のステージへ（幕）。LevelInfo が無ければ、このレベルを最初から
		isDecided_ = (level != nullptr) ? level->LoadNextLevel() : SceneTransition::ReloadScene();
	} else if (itemName == "Title") {
		// タイトルへ（幕）。ステージ選択は開かない
		isDecided_ = (level != nullptr) ? level->LoadTitle() : SceneTransition::LoadScene("Title");
	}
}

//=============================================================================
// 文字
//=============================================================================
TextComponent* ClearMenu::FindText(const std::string& name) const {
	const GameObject target = gameObject.FindChild(name);
	return target ? target.GetComponent<TextComponent>() : nullptr;
}