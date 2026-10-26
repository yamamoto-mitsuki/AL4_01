# 段階3：クリア画面・昇る粒・ステージ選択の Back（写経用）

設計：[OutGame.md](OutGame.md)（0章 見た目の決まり・5章 ステージ選択・6章 クリア画面）。前の段階：[OutGame2.md](OutGame2.md)

参考：
- エンジン：`Components/DepthOfFieldComponent.h`（値）・`Graphics/PostEffect/DepthOfFieldPass.cpp`（`CalcFocusDistance`：ピントの奥行きの測り方。同じ測り方をゲーム側に書いた）・`Systems/CameraSystem.h`（`GetMainCamera`・`GetFollowTargetTransform`）・`ECS/EntityManager.h`（`ForEach`）・`Systems/ParticleSystem.h`（`Play`）・`Components/ParticleSystemComponent.h`（`gravity`・`drag`）・`Components/UIScreenComponent.h`（Slide・Stagger）
- C++：`std::lerp`（C++20。`<cmath>`）・`virtual` の上書き（`CollectItems`）・if の中で変数を作る（`if (auto x = …; 条件)`）

## この段階でやること

ユーザーの希望（2026-10-26）：
1. **ゴールの演出**：パーティクルが上へふわふわ昇っていく
2. **クリア画面**：画面遷移（幕）なしで、**背景が強くぼけて**、その上に画面が出る
3. **ステージ選択**：タイトルへ戻れない → 画面に **Back** を出す

| パート | ファイル | 新／変 | 中身 |
|---|---|---|---|
| A | `Source/Scripts/UI/MenuBase.h / .cpp` | 2か所 | `CollectItems` を virtual に・格子で真下が無いとき、下の段の一番後ろへ |
| B | `Source/Scripts/UI/StageSelectMenu.h / .cpp` | **全部差し替え** | マスの後ろに Back（タイトルへ戻る項目） |
| C | `Source/Scripts/LevelInfo.h / .cpp` | 2か所 | `LoadNextLevel` が成功を返す・`LoadTitle` |
| D | `Source/Scripts/Effects/DepthOfFieldFade.h / .cpp` | 新 | 被写界深度を今の値から「強くぼけた値」へゆっくり動かす |
| E | `Source/Scripts/GoalDoor.h / .cpp` | **全部差し替え** | 触れたら：粒を昇らせる・ぼかし始める・1 秒後にクリア画面（幕なし） |
| F | `Source/Scripts/UI/ClearMenu.h / .cpp` | 新 | クリア画面（ステージ名・タイム・New Best・Next / Title） |
| G | VS のプロジェクト | — | 新しいファイルを入れる |
| H | エディタ | 新・変 | StageSelect に Back・Clear.uiscreen・ゴールの ClearParticle・Level02 に Depth Of Field |

**写す順番**：A → B → C → D → E → F → G（ここでビルド）→ H。

**見た目の決まり（0章）を守った所**：「！」なし・言葉は短い英語（Back・New Best・Best・Next・Title）・動きはフェードと 12px のスライドだけ・文字の後ろに Image を敷かない（読みやすさは被写界深度のぼかしで出す）・ヒットストップ・数え上げ・跳ねる動きは無し。

---

## パート A：`Source/Scripts/UI/MenuBase.h / .cpp`（2か所）

### `MenuBase.h`：`CollectItems` を virtual に
前：
```cpp
	// 項目の並び（子の Items の、有効な子。Hierarchy の上から順）
	std::vector<Handle<Entity>> CollectItems() const;
```
後：
```cpp
	// 項目の並び（子の Items の、有効な子。Hierarchy の上から順）
	// Items の外の物も項目にしたい画面（ステージ選択の Back）は、派生クラスで書き換えて後ろに足す
	virtual std::vector<Handle<Entity>> CollectItems() const;
```

### `MenuBase.cpp`：`MoveByInput` の最後（格子の ↓）
前：
```cpp
	if (down && selected_ + columns_ < count) {
		selected_ += columns_;
	}
	return selected_ != before;
```
後：
```cpp
	if (down) {
		const int lastRow = (count - 1) / columns_;
		if (selected_ + columns_ < count) {
			selected_ += columns_;
		} else if (selected_ / columns_ < lastRow) {
			// 真下に項目が無い（下の段が短い）：下の段の一番後ろへ（マスの下に置いた Back へ、どの列からでも行ける）
			selected_ = count - 1;
		}
	}
	return selected_ != before;
```

### 解説
- ステージ選択の項目は「マス 10 個＋Back」の 11 個。Columns 5 だと、Back は3段目に1つだけ。前のコードだと、真下に項目があるのは 06 だけで、07〜10 から ↓ を押しても Back へ行けなかった。
- 「真下が無ければ、下の段の一番後ろへ」は、格子のメニューでよくある決まり。ポーズ・タイトル（Columns 1）には関係ない。
- `Update` は MenuBase の物（`final`）のままだが、中で呼ぶ `CollectItems` が virtual になったので、ステージ選択では `StageSelectMenu::CollectItems`（Back を足した方）が呼ばれる。

---

## パート B：`Source/Scripts/UI/StageSelectMenu.h / .cpp`（全部差し替え）

### `StageSelectMenu.h`
```cpp
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
```

### `StageSelectMenu.cpp`
```cpp
#include "StageSelectMenu.h"

#include <algorithm>
#include <format>
#include <optional>

#include <MyEngine/Diagnostics/LogManager.h>

#include "Source/Scripts/Progress/GameProgress.h"
#include "Source/Scripts/Progress/OutGameState.h"
#include "Source/Scripts/Progress/StageCatalog.h"

SCRIPT(StageSelectMenu, "UI");

namespace {
// そのステージのシーンがあるか（一覧に書いたが、まだ作っていないステージは入れない）
bool HasStageScene(size_t index) { return index < kStageCount && SceneManager::HasScene(std::string(kStages[index].scene)); }
} // namespace

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void StageSelectMenu::Fields(ComponentUI& ui) {
	MenuBase::Fields(ui);
	ui.Label("マスは、ステージの一覧（StageCatalog.h）の数だけ、Cell Template を写して Items の下に作る（並べ方は Columns と Cell Spacing）");
	ui.Field("Screen Name", screenName_, Tip("この画面の名前（resources/ui/<名前>.uiscreen）。閉じるときに使う"));
	ui.Field("Cell Template", templateName_, Tip("マスの見本の Text の名前（一番上の直下の子）。Text の Enabled は外して置く。この位置が1つ目（左上）のマスになる"));
	ui.Field("Back Name", backName_, Tip("タイトルへ戻る項目の Text の名前（一番上の直下の子。Items の中には置かない）。マスの一番下の段で ↓ を押すと選べる。無ければ戻るキーだけ"));
	ui.Field("Cell Spacing", cellSpacing_, Tip("マスとマスの間（ピクセル）。x：右へ、y：下へ"));
	ui.Field("Cleared Color", clearedColor_, Tip("クリアしたマスの色"));
	ui.Field("Open Color", openColor_, Tip("遊べるが、まだクリアしていないマスの色"));
	ui.Field("Locked Color", lockedColor_, Tip("まだ遊べないマスの色"));
	ui.Field("Stage Name", stageNameName_, Tip("選んでいるステージの名前を書く Text の名前（一番上の直下の子）"));
	ui.Field("Best Time", bestTimeName_, Tip("選んでいるステージの1位のタイムを書く Text の名前（一番上の直下の子）"));
	ui.Field("Best Label", bestLabel_, Tip("タイムの前に付ける言葉"));
	ui.Field("No Record Text", noRecordText_, Tip("まだクリアしていないときに、タイムの所に出す文字"));
	ui.Field("Locked Text", lockedText_, Tip("まだ遊べないときに、タイムの代わりに出す言葉"));
	ui.Field("Coming Text", comingText_, Tip("シーンがまだ無いときに、タイムの代わりに出す言葉"));
	ui.Field("Locked Hint", lockedHint_, Tip("まだ遊べないマスを決めたときに、しばらく出す言葉"));
	ui.Field("Hint Time", hintTime_, AtLeast(0.0f).Tip("Locked Hint を出しておく秒数"));
}

//=============================================================================
// 最初
//=============================================================================
void StageSelectMenu::OnStart() {
	CreateCells();
	// --- 最初に選ぶマス：最後に遊んだステージ。まだ遊べなければ、遊べる中で一番後ろ ---
	size_t first = (gOutGame.lastStage < kStageCount) ? gOutGame.lastStage : 0;
	while (first > 0 && !GameProgress::IsUnlocked(first)) {
		--first;
	}
	Select(static_cast<int>(first));
}

void StageSelectMenu::CreateCells() {
	const GameObject items = gameObject.FindChild("Items");
	const GameObject cellTemplate = gameObject.FindChild(templateName_);
	const TextComponent* templateText = cellTemplate ? cellTemplate.GetComponent<TextComponent>() : nullptr;
	if (!items || templateText == nullptr) {
		LogManager::Warning("ステージ選択：子の Items か、Cell Template（Text）がありません");
		return;
	}
	const Vector3 origin = cellTemplate.GetTransform().translation; // 1つ目（左上）のマスの位置
	const int columns = (std::max)(GetColumns(), 1);

	for (size_t i = 0; i < kStageCount; ++i) {
		// --- 見本の Text を写して、文字と色だけ変える（大きさ・フォント・飾りは見本のまま）---
		TextComponent text = *templateText;
		text.enabled = true;
		text.text = std::format("{:02}", i + 1);
		if (GameProgress::IsCleared(i)) {
			text.color = clearedColor_;
		} else if (GameProgress::IsUnlocked(i) && HasStageScene(i)) {
			text.color = openColor_;
		} else {
			text.color = lockedColor_;
		}
		// --- 格子に並べる（左から右、端まで行ったら次の段。段は下へ）---
		const int column = static_cast<int>(i) % columns;
		const int row = static_cast<int>(i) / columns;
		GameObject cell = GameObject::Create(std::format("Stage{:02}", i + 1), items);
		cell.GetTransform().translation = {origin.x + cellSpacing_.x * column, origin.y - cellSpacing_.y * row, origin.z};
		cell.AddComponent(text); // 付くのは次のフレームの頭（MenuBase は、付いてから元の飾りを覚える）
	}
}

//=============================================================================
// 毎フレーム
//=============================================================================
void StageSelectMenu::OnUpdate() {
	if (hintTimer_ <= 0.0f) {
		return;
	}
	// --- Locked Hint を出し終えたら、いつもの言葉に戻す（止めない時間で数える）---
	hintTimer_ -= Time::GetUnscaleDeltaTime();
	if (hintTimer_ <= 0.0f) {
		OnSelect(GetSelected());
	}
}

//=============================================================================
// 選ぶ・決める・戻る
//=============================================================================
void StageSelectMenu::OnSelect(int index) {
	hintTimer_ = 0.0f;
	// --- Back：ステージの2行は消しておく（どのステージも選んでいないので）---
	if (IsBackIndex(index)) {
		SetText(stageNameName_, "");
		SetText(bestTimeName_, "");
		return;
	}
	const size_t stage = static_cast<size_t>(index);
	// --- 名前 ---
	SetText(stageNameName_, std::string(kStages[stage].title));
	// --- タイム（遊べない・シーンが無いときは、その言葉）---
	if (!GameProgress::IsUnlocked(stage)) {
		SetText(bestTimeName_, lockedText_);
	} else if (!HasStageScene(stage)) {
		SetText(bestTimeName_, comingText_);
	} else {
		const std::optional<float> best = GameProgress::GetBestTime(stage);
		SetText(bestTimeName_, std::format("{}  {}", bestLabel_, best ? FormatTime(*best) : noRecordText_));
	}
}

bool StageSelectMenu::CanDecide(int index) const {
	if (IsBackIndex(index)) {
		return true; // Back はいつでも
	}
	const size_t stage = static_cast<size_t>(index);
	return GameProgress::IsUnlocked(stage) && HasStageScene(stage);
}

void StageSelectMenu::OnDecideRejected(int index) {
	// まだ遊べないマス：しばらく、開け方を出す（シーンが無いマスは、もう Coming Text を出している）
	if (!GameProgress::IsUnlocked(static_cast<size_t>(index))) {
		SetText(bestTimeName_, lockedHint_);
		hintTimer_ = hintTime_;
	}
}

void StageSelectMenu::Decide(int index, const std::string& /*itemName*/) {
	// --- Back：戻るキーと同じ（閉じてタイトルへ）---
	if (IsBackIndex(index)) {
		OnCancel();
		return;
	}
	const size_t stage = static_cast<size_t>(index);
	gOutGame.lastStage = stage;
	isDecided_ = SceneTransition::LoadScene(std::string(kStages[stage].scene));
}

void StageSelectMenu::OnCancel() {
	UIScreen::Close(screenName_); // 閉じると、タイトルの項目がまた動く
	isDecided_ = true;
}

//=============================================================================
// 項目（マスの後ろに Back）
//=============================================================================
std::vector<Handle<Entity>> StageSelectMenu::CollectItems() const {
	std::vector<Handle<Entity>> items = MenuBase::CollectItems(); // マス（Items の子。一覧の数だけ）
	// Back は Items の外に置く（Items の中だと、コードで作ったマスより前に並んで、番号がずれるので）
	if (const GameObject back = gameObject.FindChild(backName_); back && back.IsActiveInHierarchy()) {
		items.push_back(back.handle);
	}
	return items;
}

bool StageSelectMenu::IsBackIndex(int index) { return static_cast<size_t>(index) >= kStageCount; }

//=============================================================================
// 文字
//=============================================================================
void StageSelectMenu::SetText(const std::string& name, const std::string& text) const {
	if (const GameObject target = gameObject.FindChild(name)) {
		if (TextComponent* component = target.GetComponent<TextComponent>()) {
			component->text = text;
		}
	}
}
```

### 変わった所
| 場所 | 前 | 後 |
|---|---|---|
| 値 | — | **Back Name**（初めは `Back`）。タイトルへ戻る項目の Text の名前 |
| `CollectItems`（新） | — | マス（Items の子）の後ろに Back を足す。Back が無ければマスだけ（今までどおり、戻るキーだけ） |
| `IsBackIndex`（新） | — | 項目の番号が、一覧の数（kStageCount）以上なら Back |
| `OnSelect` | — | Back を選んだら、下の2行（名前・タイム）を空にする |
| `CanDecide` | — | Back はいつでも決められる |
| `Decide` | — | Back なら `OnCancel`（戻るキーと同じ。閉じるとタイトルの文字が戻る） |

### 解説：なぜ Back を Items の中に置かないか
- 項目の順番は「Items の子の、Hierarchy の上から順」。マスは開いたときにコードで作るので、**ファイルに置いた Back の方が先**に並び、Back が 0 番・マスが 1〜10 番になってしまう（マスの番号＝ステージの番号、が崩れる）。
- そこで Back は Items の外（一番上の直下）に置き、`CollectItems` を書き換えて**マスの後ろに足す**。こうすると、マスの番号はそのまま、Back はいつも一番後ろ（10 個なら 10 番）。
- 選んだ見た目（SelectedStyle）・マウスの当たり（文字のかたまり）は、MenuBase がほかの項目と同じに扱う。

### 前のコードで戻れなかったわけ
前のコードも、**Esc・パッドの B・右クリック**で閉じる作りだった。でも画面にそれが出ていないので、知らないと戻れない。Back を画面に出して、キー（↓ で選んで決める）でもマウスでも戻れるようにした。
- もし前のコードで Esc を押しても戻れなかったなら、別の原因がある。そのときは教えてほしい。

---

## パート C：`Source/Scripts/LevelInfo.h / .cpp`（2か所）

### `LevelInfo.h`
前：
```cpp
	// 次のステージへ（幕）。無ければタイトルへ
	void LoadNextLevel() const;
```
後：
```cpp
	// 次のステージへ（幕）。無ければタイトルへ。幕を始められたら true（もう幕の途中なら false）
	bool LoadNextLevel() const;
	// タイトルへ（幕。Title Scene のシーン）。幕を始められたら true
	bool LoadTitle() const;
```

### `LevelInfo.cpp`：`LoadNextLevel` を差し替えて、`LoadTitle` を足す
前：
```cpp
void LevelInfo::LoadNextLevel() const {
	// 次が無ければ（最後のステージ・まだ作っていない）タイトルへ
	const std::string scene = HasNextLevel() ? std::string(kStages[GetStageIndex() + 1].scene) : titleScene_;
	SceneTransition::LoadScene(scene);
}
```
後：
```cpp
bool LevelInfo::LoadNextLevel() const {
	// 次が無ければ（最後のステージ・まだ作っていない）タイトルへ
	const std::string scene = HasNextLevel() ? std::string(kStages[GetStageIndex() + 1].scene) : titleScene_;
	return SceneTransition::LoadScene(scene);
}

bool LevelInfo::LoadTitle() const { return SceneTransition::LoadScene(titleScene_); }
```
- クリア画面（パート F）の Next / Title が、決めた後に `isDecided_`（もう一度決めない）を入れるのに、成功したかを使う。タイトルのシーンの名前は LevelInfo の Title Scene の1か所で持つ。

---

## パート D：`Source/Scripts/Effects/DepthOfFieldFade.h / .cpp`（新しいファイル）

`Source/Scripts/Effects` のフォルダを新しく作って入れる。

### `DepthOfFieldFade.h`
```cpp
#pragma once
#include <MyEngine/Core/Handle.h>
#include <MyEngine/ECS/Entity.h>

/// <summary>
/// シーンの被写界深度（Depth Of Field）の値を、今の値から決めた値へ、ゆっくり動かす（クリアの瞬間に、ゲームの絵をぼかしていく）
/// <para>Script のメンバーに持って、Begin で始め、毎フレーム Update を呼ぶ。Component ではない（保存しない）</para>
/// <para>ピントの合わせ方は Distance に切り替える。FollowTarget なら、今ピントが合っている奥行きから始める（切り替えた瞬間に、ぼけ方が跳ばないように）</para>
/// <para>シーンを読み直すと、Depth Of Field もファイルの値に戻る（戻す処理は要らない）</para>
/// </summary>
class DepthOfFieldFade {
public:
	/// <summary>
	/// 動かす値（Depth Of Field Component の同じ名前の値）
	/// </summary>
	struct Settings {
		float focusDistance = 2.0f;  // ピントの合う奥行き（m）
		float focusRange = 0.0f;     // ぼけない幅（m）
		float farTransition = 4.0f;  // 奥が一番ぼけるまでの奥行き（m）
		float nearTransition = 0.0f; // 手前が一番ぼけるまでの奥行き（m）
		float maxBlur = 20.0f;       // 一番ぼけた所の半径（ピクセル）
	};

	// 今の値から to へ、duration 秒で動かし始める。シーンに Depth Of Field が無ければ false（何もしない）
	bool Begin(const Settings& to, float duration);
	// 毎フレーム（deltaTime は、ゲームと同じに止まる時間でも、止めない時間でもよい）
	void Update(float deltaTime);
	bool IsPlaying() const { return isPlaying_; }

private:
	Handle<Entity> entity_;  // 動かす Depth Of Field の Entity（エンジンが使うのと同じ、有効な物の最初の1つ）
	Settings from_;          // 始めの値
	Settings to_;            // 終わりの値
	float duration_ = 1.0f;  // 動かす秒数
	float elapsed_ = 0.0f;   // 始めてからの秒数
	bool isPlaying_ = false; // 動かしている途中
};
```

### `DepthOfFieldFade.cpp`
```cpp
#include "DepthOfFieldFade.h"

#include <algorithm>
#include <cmath>

#include <MyEngine/Camera/Camera.h>
#include <MyEngine/Components/DepthOfFieldComponent.h>
#include <MyEngine/Components/TransformComponent.h>
#include <MyEngine/ECS/EntityManager.h>
#include <MyEngine/Math/Easing.h>
#include <MyEngine/Systems/CameraSystem.h>

namespace {
// メインカメラから point までの奥行き（カメラの前の向きに測った長さ。エンジンがピントを測るのと同じ測り方）
float CalcDepthFromCamera(const Vector3& point) {
	const Camera* camera = CameraSystem::GetMainCamera();
	// ビュー行列の3列目が、カメラの前の向き
	const Matrix4x4& view = camera->GetViewMatrix();
	const Vector3 forward = {view.m[0][2], view.m[1][2], view.m[2][2]};
	return (std::max)(Dot(point - camera->GetTranslation(), forward), 0.0f);
}
} // namespace

//=============================================================================
// 始める
//=============================================================================
bool DepthOfFieldFade::Begin(const Settings& to, float duration) {
	// --- 動かす Depth Of Field を探す（有効な物の最初の1つ。エンジンが絵に使うのも、この1つ）---
	entity_ = {};
	EntityManager::ForEach<DepthOfFieldComponent>([this](Handle<Entity> entity, const DepthOfFieldComponent& component) {
		if (!entity_.IsValid() && component.enabled && EntityManager::IsActiveInHierarchy(entity)) {
			entity_ = entity;
		}
	});
	DepthOfFieldComponent* depthOfField = EntityManager::Get<DepthOfFieldComponent>(entity_);
	if (depthOfField == nullptr) {
		isPlaying_ = false;
		return false;
	}
	// --- ピントを Distance に切り替える（FollowTarget なら、今ピントが合っている奥行きを Focus Distance に入れてから）---
	if (depthOfField->focus == DepthOfFieldFocus::FollowTarget) {
		if (const TransformComponent* target = CameraSystem::GetFollowTargetTransform()) {
			depthOfField->focusDistance = CalcDepthFromCamera(GetWorldPosition(*target));
		}
		depthOfField->focus = DepthOfFieldFocus::Distance;
	}
	// --- 今の値を覚えて、動かし始める ---
	from_ = {
	    .focusDistance = depthOfField->focusDistance,
	    .focusRange = depthOfField->focusRange,
	    .farTransition = depthOfField->farTransition,
	    .nearTransition = depthOfField->nearTransition,
	    .maxBlur = depthOfField->maxBlur,
	};
	to_ = to;
	duration_ = (std::max)(duration, 0.01f); // 0 で割らないように
	elapsed_ = 0.0f;
	isPlaying_ = true;
	return true;
}

//=============================================================================
// 毎フレーム
//=============================================================================
void DepthOfFieldFade::Update(float deltaTime) {
	if (!isPlaying_) {
		return;
	}
	elapsed_ = (std::min)(elapsed_ + deltaTime, duration_);
	// ゆっくり始まって、ゆっくり止まる（InOutSine）
	const float t = Ease::Apply(elapsed_ / duration_, Ease::Type::InOutSine);
	if (DepthOfFieldComponent* depthOfField = EntityManager::Get<DepthOfFieldComponent>(entity_)) {
		depthOfField->focusDistance = std::lerp(from_.focusDistance, to_.focusDistance, t);
		depthOfField->focusRange = std::lerp(from_.focusRange, to_.focusRange, t);
		depthOfField->farTransition = std::lerp(from_.farTransition, to_.farTransition, t);
		depthOfField->nearTransition = std::lerp(from_.nearTransition, to_.nearTransition, t);
		depthOfField->maxBlur = std::lerp(from_.maxBlur, to_.maxBlur, t);
	}
	isPlaying_ = (elapsed_ < duration_);
}
```

### 解説
- **何をするか**：ゴールに触れた瞬間の被写界深度（Level01 なら「キューブにピント・奥が少しぼける」）から、**「カメラの 2m 先にピント・奥は 4m で一番ぼける・ぼけの半径 20」**（＝遊ぶ面も背景も全部強くぼけた状態）へ、1.4 秒でなめらかに動かす。タイトルの背景のぼかし（Focus Distance 2・Focus Range 0・Far Transition 4）と同じ考え方で、ぼけを強くした物。
- **ピントの切り替えで跳ばない工夫**：Level01 の Depth Of Field は FollowTarget（キューブに合わせる）。そのまま Distance に切り替えると、Focus Distance の値（16.5 のまま）にピントが跳ぶ。そこで、**今キューブにピントが合っている奥行きを、エンジンと同じ測り方で求めて**（カメラの前の向きと「カメラ → キューブ」の内積）Focus Distance に入れてから切り替える。見た目は変わらず、そこから動き出す。
- **ぼけの強さ**：Max Blur の上限はエンジンで 32（`DepthOfFieldPass::kMaxBlurLimit`）。20 で「強くぼけた」に見えた（下の「作業用コピーで確かめたこと」）。もっと強くするなら GoalDoor の Blur Max を上げる（重くなる）。
- **元に戻す処理は無い**：値を変えるのは実行中の Component だけ。Next・Title・やり直しでシーンを読み直すと、ファイルの値に戻る。エディタも Stop で戻る。
- `std::lerp(a, b, t)` は C++20 の「a から b へ t の割合」。`Ease::Apply(t, InOutSine)` で、ゆっくり始まってゆっくり止まる。
- Component ではなく、ただのクラス（保存しない）。GoalDoor がメンバーに1つ持つ。

---

## パート E：`Source/Scripts/GoalDoor.h / .cpp`（全部差し替え）

### `GoalDoor.h`
```cpp
#pragma once
#include <string>

#include <MyEngine/ECS/Script.h>

#include "Source/Scripts/Effects/DepthOfFieldFade.h"

/// <summary>
/// ゴールの扉。Collider（Is Trigger）を付けた Entity に付ける
/// <para>氷のキューブ（IceCube）が触れたらクリア：キューブが扉へ吸い込まれ、子の Clear Particle（Particle System）の粒がふわふわ昇り始め、
/// ゲームの絵がゆっくりぼけていく（被写界深度）。Screen Delay 秒たったら、クリア画面（UI Screen）を出す。シーンは替えない</para>
/// <para>クリア画面が無いとき（ファイルが無い）は、次のレベルへ（LevelInfo）</para>
/// </summary>
class GoalDoor : public Script {
public:
	void Update() override;
	void Fields(ComponentUI& ui) override;
	void OnTriggerEnter(GameObject other) override; // キューブが触れた

private:
	// ===== Inspector に出す・保存する値 =====
	// --- クリア画面 ---
	std::string clearScreen_ = "Clear"; // 出す画面の名前（resources/ui/<名前>.uiscreen）
	float screenDelay_ = 1.0f;          // 触れてから、クリア画面を出すまでの秒数（キューブが扉へ吸い込まれる間）
	// --- 昇る粒 ---
	std::string particleName_ = "ClearParticle"; // 触れたときに流し始める Particle System の名前（この Entity の直下の子。Play On Start は外して置く）
	// --- ぼかし（被写界深度。触れた瞬間から、今の値をこの値へ動かす）---
	float blurTime_ = 1.4f;              // ぼけきるまでの秒数
	DepthOfFieldFade::Settings blurTo_;  // ぼけきった時の値（初めは、カメラの 2m 先より奥が全部ぼける）

	// ===== 実行中だけの値（保存しない）=====
	bool isCleared_ = false;         // キューブが触れた
	float timer_ = 0.0f;             // 触れてからの秒数
	bool isScreenRequested_ = false; // クリア画面を頼んだ（何回も頼まないように）
	DepthOfFieldFade blur_;          // ぼかしの動き
};
```

### `GoalDoor.cpp`
```cpp
#include "GoalDoor.h"

#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/Systems/ParticleSystem.h>

#include "IceCube.h"
#include "LevelInfo.h"

SCRIPT(GoalDoor, "Game");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void GoalDoor::Fields(ComponentUI& ui) {
	ui.Label("キューブが触れたら：粒が昇る・ゲームの絵がぼけていく → Screen Delay 秒後にクリア画面（シーンは替えない）");
	// --- クリア画面 ---
	ui.Field("Clear Screen", clearScreen_, Tip("出す画面の名前（resources/ui の .uiscreen の名前）。無ければ、次のレベルへ進む"));
	ui.Field("Screen Delay", screenDelay_, AtLeast(0.0f).Tip("キューブが触れてから、クリア画面を出すまでの秒数（キューブが扉へ吸い込まれる間）"));
	// --- 昇る粒 ---
	ui.Field("Particle Name", particleName_, Tip("触れたときに流し始める Particle System の名前（この Entity の直下の子）。Play On Start を外して置く"));
	// --- ぼかし ---
	ui.Field("Blur Time", blurTime_, AtLeast(0.01f).Tip("触れてから、ゲームの絵がぼけきるまでの秒数（ゆっくり始まって、ゆっくり止まる）"));
	ui.Field("Blur Focus Distance", blurTo_.focusDistance, AtLeast(0.0f).Tip("ぼけきった時のピントの奥行き（m）。小さいほど、遊ぶ面まで全部ぼける"));
	ui.Field("Blur Focus Range", blurTo_.focusRange, AtLeast(0.0f).Tip("ぼけきった時の、ぼけない幅（m）。0 で、ピントの所もすぐぼけ始める"));
	ui.Field("Blur Far Transition", blurTo_.farTransition, AtLeast(0.0f).Tip("ぼけきった時の、一番ぼけるまでの奥行き（m）。小さいほど、すぐ奥が強くぼける"));
	ui.Field("Blur Near Transition", blurTo_.nearTransition, AtLeast(0.0f).Tip("ぼけきった時の、手前のぼけ（m）。0 なら手前はぼかさない"));
	ui.Field("Blur Max", blurTo_.maxBlur, AtLeast(0.0f).Tip("ぼけきった時の、ぼけの半径（ピクセル。エンジンの上限は 32）。大きいほど強くぼける"));
}

//=============================================================================
// 当たり
//=============================================================================
void GoalDoor::OnTriggerEnter(GameObject other) {
	// --- 早期リターン：キューブでない・もうやられた・もうクリアした ---
	IceCube* cube = other.GetComponent<IceCube>();
	if (cube == nullptr || !cube->IsPlayable() || isCleared_) {
		return;
	}
	// --- クリア：キューブを扉へ吸い込ませて、時間を数え始める ---
	cube->Clear(GetWorldPosition(transform));
	isCleared_ = true;
	// --- レベルにクリアを知らせる（時間を止めて、進み具合に記録する）---
	if (LevelInfo* level = LevelInfo::Find()) {
		level->Clear();
	}
	// --- 粒が昇り始める ---
	if (const GameObject particle = gameObject.FindChild(particleName_)) {
		ParticleSystem::Play(particle.handle);
	}
	// --- ゲームの絵をぼかし始める ---
	if (!blur_.Begin(blurTo_, blurTime_)) {
		LogManager::Warning("GoalDoor：シーンに Depth Of Field が無いので、ぼかしません（Main Camera に付けると、クリアの時にぼける）");
	}
}

//=============================================================================
// 毎フレーム
//=============================================================================
void GoalDoor::Update() {
	if (!isCleared_) {
		return;
	}
	const float deltaTime = Time::GetDeltaTime();
	blur_.Update(deltaTime);
	if (isScreenRequested_) {
		return;
	}
	timer_ += deltaTime;
	if (timer_ < screenDelay_) {
		return;
	}
	// --- クリア画面を出す（画面が無ければ、次のレベルへ。LevelInfo も無ければ、このレベルを最初から）---
	isScreenRequested_ = true;
	if (UIScreen::Open(clearScreen_)) {
		return;
	}
	LogManager::Warning("GoalDoor：クリア画面を出せなかったので、次のレベルへ進みます（resources/ui/" + clearScreen_ + ".uiscreen が無い？）");
	if (const LevelInfo* level = LevelInfo::Find()) {
		level->LoadNextLevel();
	} else {
		SceneManager::ReloadScene();
	}
}
```

### 変わった所
| 前 | 後 |
|---|---|
| 触れて 1 秒後に、次のレベルへ（幕） | 触れたら：**ClearParticle の粒が昇り始める・ゲームの絵がぼけ始める**。1 秒後に**クリア画面を出す（幕なし。シーンはそのまま）** |
| 値：Next Level Delay | 値：Clear Screen・Screen Delay・Particle Name・Blur Time・Blur 〜（ぼけきった時の値） |
| — | クリア画面のファイルが無ければ、今までどおり次のレベルへ（ゲームが止まらないように） |

- 時間は `Time::GetDeltaTime()`（ゲームと同じ時間）。クリア画面は Pause Game を外すので、ゲームは止まらず、粒もぼかしも動き続ける。
- **Next Level Delay の名前が変わった**ので、Level01・Level02 を開くと Console に「ファイルの項目 "Next Level Delay" を読む所がありません」が出る。パート H で値を入れて保存し直せば消える。

---

## パート F：`Source/Scripts/UI/ClearMenu.h / .cpp`（新しいファイル）

### `ClearMenu.h`
```cpp
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
	Vector4 newBestColor_ = {0.85f, 0.95f, 1.0f, 1.0f};     // 文字の色
	float newBestGlowWidth_ = 8.0f;                         // うっすら光る幅
	Vector4 newBestGlowColor_ = {0.75f, 0.93f, 1.0f, 0.55f}; // 光の色（氷色）
	// --- 入力 ---
	float inputDelay_ = 1.0f;          // 開いてから、入力を受けるまでの秒数（項目が見えてくるまで）
	std::string nextItemName_ = "Next"; // 次のステージへの項目の名前（次が無ければ消す）

	// ===== 実行中だけの値 =====
	float elapsed_ = 0.0f; // 開いてからの秒数（止めない時間で数える）
};
```

### `ClearMenu.cpp`
```cpp
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

void ClearMenu::OnUpdate() {
	elapsed_ += Time::GetUnscaleDeltaTime();
}

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
```

### 解説
- **出す文字**：ステージの名前（`kStages` の title）・遊んだ時間（`FormatTime`。段階2 で作った `"0:32.45"` の形）・記録。記録は、1位になった（初めてのクリアも）なら **New Best**（氷色でうっすら光る）、ならなかったら **Best  0:30.12**（前の1位のタイムを、Record Text の薄い色のまま）。
- **時間は LevelInfo が止めた物**：ゴールに触れた瞬間に `LevelInfo::Clear` で止めて記録している（段階2）ので、クリア画面は読むだけ。
- **入力を受けない時間（Input Delay 1 秒）**：文字は上から 0.25 秒ずつ遅れて出てくる（UI Screen の Stagger）。Next / Title が見えてくる前に Enter を押しても、決まらない。
- **次のステージが無いとき**：Next を `SetActive(false)`。MenuBase は有効な項目だけ集めるので、Title が1つ目になる。
- **Esc は何もしない**：クリアした後は LevelInfo が Esc・R を見ない（段階2）。クリア画面でも戻る先が無いので、`OnCancel` は書かない。
- 設計（OutGame.md 6章）にあった「決めるキーで演出を飛ばす」は**入れなかった**：UI Screen の出す動きを途中で終わらせる関数がエンジンに無いため（エンジンは変えない）。全部出るまで約 1.6 秒・入力は 1 秒からなので、待たされる感じは少ない。

---

## パート G：VS のプロジェクト

| フォルダ（フィルター） | 入れるファイル |
|---|---|
| Source/Scripts/Effects（新しく作る） | `DepthOfFieldFade.h`・`DepthOfFieldFade.cpp` |
| Source/Scripts/UI | `ClearMenu.h`・`ClearMenu.cpp` |

ソリューションエクスプローラーで右クリック → 追加 → **既存の項目**（入れ忘れると `DepthOfFieldFade::Begin` などが未解決のリンクエラー）。

---

## パート H：エディタ

### H-1. StageSelect.uiscreen に Back を足す
`resources/ui/StageSelect.uiscreen` を開いて（Edit UI Screen）：

| Entity | 親 | Component・値 |
|---|---|---|
| **Back** | **StageSelect（一番上）の直下**。Items の中には入れない | Text「Back」・Noto Sans JP・Font Size 28・Color (1, 1, 1, **0.6**)・位置 **(0, -300)** |

- 一番簡単なのは、Header を右クリック → Duplicate して、名前・文字・位置・大きさ・色を変える。
- 位置は好みで（画面の左下 (-560, -300) なども）。↓ で行けるのは位置に関係なく「マスの一番下の段の、下」。
- Stage Select Menu の **Back Name** が `Back` になっているか見る（コードの初めの値なので、そのままなら入っている）。
- Hierarchy で Back を一番下に置くと、UI Screen の Stagger で一番最後に出てくる。

### H-2. Clear.uiscreen を作る
1. Project ウィンドウの `resources/ui` で **Create → UI Screen** → 名前 `Clear`（**この名前は GoalDoor の Clear Screen と合わせる**）。
2. 開いて、下のように作る。**名前はコードが探すので、このとおりに**。

| Entity | 親 | Component・値 |
|---|---|---|
| **Clear**（一番上） | — | UI Screen：**Pause Game を外す**・Show：**Slide**（Side **Bottom**・Distance **12**・With Fade ✓・Duration **0.8**・**OutCubic**）・Show Stagger **0.25**・Hide：Fade 0.3（Linear）。Add Component → Gameplay → UI → **Clear Menu**：**Columns 2**・Hit Padding 16 |
| **StageText** | Clear | Text「Stage 01」・Font Size **26**・Color A **0.75**・Letter Spacing **4**（字間を広く）・位置 (0, 110) |
| **TimeText** | Clear | Text「0:00.00」・Font Size **84**・Color 白・位置 (0, 30) |
| **RecordText** | Clear | Text（空でよい）・Font Size **24**・Color A **0.55**・位置 (0, -50) |
| **Items** | Clear | 空の Entity・位置 (0, 0) |
| **Next** | Items | Text「Next」・Font Size 30・Color A 0.6・位置 (-90, -150) |
| **Title** | Items | Text「Title」・Font Size 30・Color A 0.6・位置 (90, -150) |
| **SelectedStyle** | Clear | Text・**Enabled を外す**・Color 白・Glow **10**・Glow Color は StageSelect の SelectedStyle と同じに（ほかは無し） |

- フォントは全部 Noto Sans JP（Title と同じ）。縁取り・影・Shine・Motion は付けない。文字の後ろに Image を置かない（背景はぼけているので読める）。
- **Hierarchy の並び順＝出てくる順**（Stagger）：StageText → TimeText → RecordText → Items（Next・Title）。0.25 秒ずつ遅れて、下から 12px 上がりながらフェードで出る。
- Clear Menu の値（New Best Text・New Best Color・Glow・Input Delay・Next Item Name）は初めの値のままでよい。
- **RecordText は 24 以上にする**：22 だと「Best」の B の縦線が消えて「3est」に見えた（作業用コピーで見た。細いフォントを小さく描くと、線が画素の間に落ちて消えることがある）。

### H-3. ゴールに ClearParticle（昇る粒）を足す ― Level01・Level02
Goal（GoalDoor の付いた Entity）を右クリック → Create Empty（子）→ 名前 **ClearParticle** → Add Component → Effects → **Particle System**：

| 見出し | 値 |
|---|---|
| Transform | 位置 (0, 0, 0)・**Scale (0.6667, 0.3333, 1)**（親の Goal の Scale (1.5, 3, 1) を打ち消して、下の大きさを m のままにする。今ある Particle System の子と同じ） |
| 再生 | **Play On Start を外す**（GoalDoor が触れたときに Play する）・Looping ✓・Duration **30**（Burst が 30 秒に1回だけになるように） |
| 出す量 | Rate Over Time **14**・Burst Count **30**・Max Particles 256 |
| Shape | **Box**・Box Size **(3, 2.5, 0.6)**（扉のまわりから出す）・From Surface は外す |
| 初めの値 | Lifetime **5〜7**・Speed **0.1〜0.5**（横へ少し散る）・Size **0.12〜0.28**・Color A **(1, 0.97, 0.85, 1)**・Color B **(0.85, 0.95, 1, 1)**（白っぽい金と氷色の間）・Intensity **3.5** |
| 生きている間 | **Gravity (0, 0.7, 0)**（上へ）・**Drag 0.5**・End Size Rate **0.5**・End Color (1, 1, 1, 0)・Ease **InCubic** |
| 見た目 | Blend Add・Space **World**（出た所から昇る）・**Use Fog を外す**・Use Depth Of Field は外したまま（ぼけた背景の上で、粒だけくっきり） |

**ふわふわ昇る仕組み**：Gravity を上向きにすると、粒は上へ加速し続ける。Drag（減速）を一緒に掛けると、**速さが「Gravity ÷ Drag」＝ 0.7 ÷ 0.5 ＝ 1.4 m/s で頭打ち**になる（加速と減速がつり合う）。だから、すっと飛んでいかず、一定のゆっくりした速さで昇り続ける。Speed の横向きの散らばりも Drag で少しずつ消えるので、出た直後だけ横にふわっと広がって、あとは真っすぐ昇る。InCubic は、寿命の終わり近くまで明るさ・大きさを保ってから消える変わり方。
- もっとゆっくり：Gravity を下げる（0.4 なら 0.8 m/s）。もっと高くまで：Lifetime を延ばす。
- 粒1つ1つが左右に揺れる動き（ゆらゆら）は、Particle System に無い（エンジンの機能が要る）。今回は「横にふわっと散ってから昇る」で作った。

### H-4. Goal の GoalDoor の値 ― Level01・Level02
| 値 | 入れる |
|---|---|
| Clear Screen | `Clear` |
| Screen Delay | 1 |
| Particle Name | `ClearParticle` |
| Blur Time | 1.4 |
| Blur Focus Distance | 2 |
| Blur Focus Range | 0 |
| Blur Far Transition | 4 |
| Blur Near Transition | 0 |
| Blur Max | **20**（強さの好みで 16〜28） |

コードの初めの値と同じ。シーンには前の値（Next Level Delay）しか入っていないので、開いたときにこの値になっているかを見る。

### H-5. Level02 の Main Camera に Depth Of Field を付ける
**Level02 には Depth Of Field が無い**ので、このままだとクリアの時にぼけない（Console に「GoalDoor：シーンに Depth Of Field が無いので、ぼかしません」）。Level01 の Main Camera の Depth Of Field と同じ値で付ける：Focus **FollowTarget**・Focus Range 3・Far Transition 100・Near Transition 4・Max Blur 8.5。
- これから作る Level03〜 も、Main Camera に Depth Of Field を付けておく（Level01 を Save As で写して作れば付いている）。

### H-6. 保存
Level01・Level02 を **Ctrl+S**（H-3〜H-5 の変更と、Next Level Delay の警告を消すため）。

---

## 確かめること（写した後）

| 何をする | こうなればよい |
|---|---|
| Title で Start | ステージ選択の一番下に **Back**（少し薄い） |
| マスの2段目（06〜10 のどれでも）で ↓ | Back が明るくなってうっすら光る。上の2行（名前・タイム）が消える |
| Back で ↑ | 06 に戻る |
| Back で決める・Back をクリック | ステージ選択が消えて、タイトルの文字が戻る。Start・Quit が選べる |
| Esc・右クリック・パッドの B | 今までどおり閉じる |
| Level01 をゴール | 扉から白っぽい粒がふわふわ昇り始め、**ゲームの絵が 1.4 秒かけて強くぼける**。幕は出ない |
| 1 秒後 | 下から少し上がりながら「Stage 01」→「0:12.34」→「New Best」（初めて・1位）→「Next  Title」が順に出る。粒は昇り続ける |
| 出始めてすぐ Enter | 何も起きない（1 秒たってから受ける） |
| Next | Level02 へ（幕）。ぼかしは元に戻っている |
| 同じステージを遅いタイムでクリア | New Best ではなく「Best  0:12.34」（前の1位）が薄く出る |
| Level02 をゴール | ぼける（H-5 を入れた場合）。次のシーンが無いので **Next が出ず、Title だけ** |
| Title | Title シーンへ（幕）。ステージ選択は開かない |
| クリア画面で Esc・R | 何も起きない |

## 作業用コピーで確かめたこと（2026-10-26）
- 今の AL4_01（段階2 を写した物）を写して、このページのコード・H の画面を入れた。**Debug・Release ともビルドが通った**（エラー 0。ゲームのファイルから警告 0）。Debug（エディタ）も起動した。
- **クリア**：Level01 を写して、キューブをゴールの中から始めるシーンを作り、製品版で撮った。触れた瞬間に粒が出て、1 秒ほどで山・森・地面まで全部強くぼけ、その上に「Stage 01 / タイム / New Best / Next  Title」が出た。2回目は「Best  0:00.01」（前の1位）になった。粒は 5 秒ほどで扉から画面の上の方まで昇った（最初に試した Gravity 0.5・Drag 0.8・Lifetime 3〜4.5 では、扉のまわりで消えてしまい「昇る」に見えなかったので、H-3 の値に直した）。
- **Back**：一時的なコードで、ステージ選択を開いて Back を選び、決めた：Back が SelectedStyle の色で光り、上の2行が消え、決めるとステージ選択が閉じてタイトルの文字が戻った。
- 扉（Goal の Primitive Renderer。Additive）と、今からある黄色い粒（Use Depth Of Field を外してある）はぼけずにくっきり残る。ぼけた世界で扉だけが光って見えるので、このままにした。扉もぼかしたいときは、扉の Surface を Additive 以外にする。
- キー・マウスで実際に選ぶ・ゴールする所は試していない（ユーザーの PC に本物の入力は送らないため）。上の表で確かめてほしい。

## 困ったとき

| 症状 | 原因 |
|---|---|
| `DepthOfFieldFade::…`・`ClearMenu` が未解決（リンクエラー） | パート G：cpp をプロジェクトに入れていない |
| `CollectItems` の override でエラー | パート A の `virtual` を書き忘れた |
| Back が出ない・1番目のマスのように動く | Back を Items の中に入れた・名前が違う |
| ↓ で Back へ行けない（06 からだけ行ける） | パート A の MenuBase.cpp を写していない |
| ゴールすると次のレベルへ進むだけ（画面が出ない） | Clear.uiscreen が無い・名前が GoalDoor の Clear Screen と違う（Console に「クリア画面を出せなかった」） |
| ぼけない | そのシーンに Depth Of Field が無い（H-5）・Depth Of Field の Enabled が外れている |
| ぼけ方が弱い | Blur Max を上げる（上限 32）・Blur Focus Distance を小さく |
| 粒が出ない | ClearParticle の名前が違う・Goal の直下でない・Particle System の Enabled が外れている |
| 粒が最初から出ている | ClearParticle の Play On Start が付いたまま |
| 粒が扉のまわりで消える | Lifetime が短い・Gravity が小さい（H-3） |
| 文字が出てくる前に決まってしまう | Clear Menu の Input Delay が 0 |
| クリア画面の間にゲームが止まる・粒が止まる | Clear の UI Screen の Pause Game が付いたまま |
