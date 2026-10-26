# 段階2：ステージの一覧・記録・ステージ選択（写経用）

設計：[OutGame.md](OutGame.md)（0章 見た目の決まり・2章 データ・5章 ステージ選択・7章 段階）。前の段階：[OutGame1.md](OutGame1.md)

参考：
- エンジン：`ECS/GameObject.h`（`Create`・`AddComponent`：Component が付くのは次のフレームの頭）・`Components/AlphaGroupComponent.h`・`Scene/SceneManager.h`（`HasScene`）・`externals/nlohmann/json.hpp`（`parse` の3つ目の引数 false で例外を投げない）
- C++：`std::to_array`（C++20。並べた数から `std::array` の大きさを決める）・`std::optional`・`inline` 変数（C++17）

## この段階でやること

| パート | ファイル | 新／変 | 中身 |
|---|---|---|---|
| A | `Source/Scripts/Progress/StageCatalog.h` | 新 | ステージの一覧（数を書かない） |
| B | `Source/Scripts/Progress/OutGameState.h` | 新 | 画面をまたぐ値（最後に遊んだステージ） |
| C | `Source/Scripts/Progress/GameProgress.h / .cpp` | 新 | クリアと1位のタイムを `saves/Progress.json` に保存・タイムの文字 |
| D | `Source/Scripts/UI/MenuBase.h / .cpp` | 少し変 | 後から Text が付く項目に対応・Columns を読めるように |
| E | `Source/Scripts/LevelInfo.h / .cpp` | **全部差し替え** | 一覧の次へ・遊んだ時間・クリアの記録 |
| F | `Source/Scripts/GoalDoor.cpp` | 1か所足す | ゴールで LevelInfo にクリアを知らせる |
| G | `Source/Scripts/UI/TitleMenu.h / .cpp` | **全部差し替え** | ステージ選択の間、タイトルの文字を静かに消す・F1 / F2 |
| H | `Source/Scripts/UI/StageSelectMenu.h / .cpp` | 新 | ステージ選択（マスを一覧の数だけ作る） |
| I | VS のプロジェクト・`.gitignore` | — | 新しいファイルを入れる・保存のフォルダを git に入れない |
| J | エディタ：`StageSelect.uiscreen`・Level01 / Level02 | 新・保存し直す | ステージ選択の画面 |

**写す順番**：A → B → C → D → E → F → I（ここでビルド。ゴールすると `project/saves/Progress.json` ができるのを確かめる）→ G → H → I → J。

**見た目の決まり（0章）**：「！」なし・短い英語・フェードと少しのスライドだけ・数字や文字の後ろに Image を置かない。この段階の言葉（Select Stage・Best・Locked・Coming soon・Clear the previous stage）は全部 Inspector で変えられる。

---

## パート A：`Source/Scripts/Progress/StageCatalog.h`（新しいファイル）

```cpp
#pragma once
#include <array>
#include <cstddef>
#include <string_view>

/// <summary>
/// ステージ1つ分（一覧の1行）
/// </summary>
struct StageEntry {
	std::string_view scene; // シーンの名前（resources/scenes/<scene>.scene.json）
	std::string_view title; // ステージ選択の下の行に出す名前
};

// ステージの一覧（遊ぶ順）。ステージを増やす・減らす・並べ替えるときは、ここの行を足す・消す・入れ替えるだけ
// 数は書かない（std::to_array が、並べた行の数から std::array の大きさを決める）。ステージ選択のマスも、この数だけ作られる
// 進み具合はシーンの名前で保存するので、並べ替えても記録はずれない（GameProgress）
inline constexpr auto kStages = std::to_array<StageEntry>({
    {"Level01", "Stage 01"},
    {"Level02", "Stage 02"},
    {"Level03", "Stage 03"},
    {"Level04", "Stage 04"},
    {"Level05", "Stage 05"},
    {"Level06", "Stage 06"},
    {"Level07", "Stage 07"},
    {"Level08", "Stage 08"},
    {"Level09", "Stage 09"},
    {"Level10", "Stage 10"},
});

// ステージの数
inline constexpr size_t kStageCount = kStages.size();
```

- **`std::to_array<StageEntry>({ … })`**：`{ }` に並べた数から、`std::array<StageEntry, 10>` を作る（C++20）。`std::array<StageEntry, 10>` と書くと、行を足すたびに 10 も直す必要があり、直し忘れると足りない分が空の行になる。
- `std::string_view` は文字を持たずに指すだけ。`"Level01"` の文字はプログラムの中にずっとあるので、`constexpr` の一覧に入れられる（`std::string` は入れられない）。
- `title`（ステージ選択の下の行に出す名前）は自由に変えてよい（「霧の入口」なども可）。
- Level03〜10 はまだシーンが無いので、ステージ選択では「Coming soon」になって入れない。シーンを作れば、そのまま入れるようになる。

---

## パート B：`Source/Scripts/Progress/OutGameState.h`（新しいファイル）

```cpp
#pragma once
#include <cstddef>

/// <summary>
/// 画面（シーン）をまたいで覚えておく値。シーンが替わると Entity は全部消えるので、シーンの外に置く
/// <para>保存はしない（ゲームを起動し直したら初めの値から）。保存する物は GameProgress</para>
/// </summary>
struct OutGameState {
	size_t lastStage = 0; // 最後に遊んだ・次に遊ぶステージ（kStages の番号。ステージ選択で、最初にここを選ぶ）
};

// ゲームに1つ（inline 変数：どの .cpp から読んでも同じ1つを指す）
inline OutGameState gOutGame;
```

- **`inline` 変数**（C++17）：ヘッダに `OutGameState gOutGame;` とだけ書くと、このヘッダを読んだ .cpp の数だけ同じ名前の変数ができて、リンクエラー（二重定義）になる。`inline` を付けると「全部の .cpp で同じ1つ」になる。
- シーンが替わると Entity（と Script）は全部消えるので、「最後に遊んだステージ」はシーンの外のここに置く。

---

## パート C：`Source/Scripts/Progress/GameProgress.h / .cpp`（新しいファイル）

### `GameProgress.h`

```cpp
#pragma once
#include <cstddef>
#include <optional>
#include <string>

/// <summary>
/// 進み具合（どのステージをクリアしたか・1位のタイム）。saves/Progress.json に保存する
/// <para>初めて読まれた時に1回ファイルを読み、変わったらすぐ書く。番号は kStages（StageCatalog.h）の番号</para>
/// <para>ファイルにはシーンの名前で書くので、kStages を並べ替える・足す・消すしても記録はずれない</para>
/// </summary>
class GameProgress {
public:
	// クリアしたか
	static bool IsCleared(size_t index);
	// 遊べるか（0 番はいつも。ほかは1つ前をクリアしたら）
	static bool IsUnlocked(size_t index);
	// 1位のタイム（秒）。まだクリアしていなければ空
	static std::optional<float> GetBestTime(size_t index);

	/// <summary>
	/// クリアした。タイムが1位なら覚える。すぐファイルに書く
	/// </summary>
	/// <returns>前の1位のタイム（初めてのクリアなら空）。新しい1位か調べるのに使う</returns>
	static std::optional<float> RecordClear(size_t index, float time);

	// ===== デバッグ（エディタのタイトルで F1・F2）=====
	static void UnlockAll(); // 全部クリアした扱いにする（タイムは変えない）
	static void ResetAll();  // 全部消す
};

// 秒を「分:秒.1/100秒」にする（32.456 → "0:32.45"。1/100 秒より下は切り捨て）
std::string FormatTime(float seconds);
```

### `GameProgress.cpp`

```cpp
#include "GameProgress.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>

#include <externals/nlohmann/json.hpp>

#include <MyEngine/Diagnostics/LogManager.h>

#include "Source/Scripts/Progress/StageCatalog.h"

namespace {
constexpr const char* kFilePath = "saves/Progress.json"; // 保存の場所（起動したフォルダから。resources と同じ所）
constexpr int kFileVersion = 1;                          // ファイルの形の版（形を変えたら上げる）

/// <summary>
/// ステージ1つ分の記録
/// </summary>
struct StageRecord {
	bool isCleared = false;          // クリアした
	std::optional<float> bestTime;   // 1位のタイム（秒。まだ無ければ空）
};

/// <summary>
/// 進み具合の中身（この .cpp の中だけ。ヘッダに nlohmann を出さないため）
/// </summary>
struct ProgressState {
	std::array<StageRecord, kStageCount> records; // kStages と同じ並び
	nlohmann::json file = nlohmann::json::object(); // 読んだファイルの中身（一覧から消したステージの記録も、消さずに書き戻すため）
	bool isLoaded = false;                          // もうファイルを読んだ
};
ProgressState state;

// 初めて呼ばれた時に1回だけ、ファイルを読む（無い・壊れている → 何もクリアしていない扱い）
void LoadIfNeeded() {
	if (state.isLoaded) {
		return;
	}
	state.isLoaded = true;
	std::ifstream stream(kFilePath);
	if (!stream) {
		return; // まだ無い（初めての起動）
	}
	nlohmann::json json = nlohmann::json::parse(stream, nullptr, false); // false：壊れていても例外を投げない（is_discarded になる）
	if (json.is_discarded() || !json.is_object()) {
		LogManager::Warning(std::string(kFilePath) + " が読めないので、何もクリアしていない扱いにします");
		return;
	}
	state.file = std::move(json);
	// --- 一覧のステージごとに、シーンの名前で記録を探す ---
	const auto stages = state.file.find("stages");
	if (stages == state.file.end() || !stages->is_object()) {
		return;
	}
	for (size_t i = 0; i < kStageCount; ++i) {
		const auto found = stages->find(std::string(kStages[i].scene));
		if (found == stages->end() || !found->is_object()) {
			continue;
		}
		StageRecord& record = state.records[i];
		record.isCleared = found->value("cleared", false);
		if (const auto time = found->find("bestTime"); time != found->end() && time->is_number()) {
			record.bestTime = time->get<float>();
		}
	}
}

// 今の記録をファイルに書く（一覧に無いステージの記録は、読んだまま残す）
void Save() {
	nlohmann::json& stages = state.file["stages"]; // 無ければ空のオブジェクトができる
	if (!stages.is_object()) {
		stages = nlohmann::json::object();
	}
	for (size_t i = 0; i < kStageCount; ++i) {
		const StageRecord& record = state.records[i];
		nlohmann::json entry = {{"cleared", record.isCleared}};
		if (record.bestTime) {
			entry["bestTime"] = *record.bestTime;
		}
		stages[std::string(kStages[i].scene)] = std::move(entry);
	}
	state.file["version"] = kFileVersion;

	// --- フォルダが無ければ作って、書く ---
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(kFilePath).parent_path(), error);
	std::ofstream stream(kFilePath);
	if (!stream) {
		LogManager::Warning(std::string(kFilePath) + " に書けませんでした");
		return;
	}
	stream << state.file.dump(4) << '\n';
}

// 番号が一覧の中か
bool IsValidIndex(size_t index) { return index < kStageCount; }
} // namespace

//=============================================================================
// 読む
//=============================================================================
bool GameProgress::IsCleared(size_t index) {
	LoadIfNeeded();
	return IsValidIndex(index) && state.records[index].isCleared;
}

bool GameProgress::IsUnlocked(size_t index) {
	if (!IsValidIndex(index)) {
		return false;
	}
	return index == 0 || IsCleared(index - 1);
}

std::optional<float> GameProgress::GetBestTime(size_t index) {
	LoadIfNeeded();
	return IsValidIndex(index) ? state.records[index].bestTime : std::nullopt;
}

//=============================================================================
// 書く
//=============================================================================
std::optional<float> GameProgress::RecordClear(size_t index, float time) {
	LoadIfNeeded();
	if (!IsValidIndex(index)) {
		return std::nullopt;
	}
	StageRecord& record = state.records[index];
	const std::optional<float> previousBest = record.bestTime;
	record.isCleared = true;
	if (!record.bestTime || time < *record.bestTime) {
		record.bestTime = time;
	}
	Save();
	return previousBest;
}

void GameProgress::UnlockAll() {
	LoadIfNeeded();
	for (StageRecord& record : state.records) {
		record.isCleared = true;
	}
	Save();
	LogManager::Log("GameProgress：全部のステージを開きました");
}

void GameProgress::ResetAll() {
	LoadIfNeeded();
	state.records = {};
	state.file = nlohmann::json::object(); // 一覧に無いステージの記録も消す
	Save();
	LogManager::Log("GameProgress：記録を全部消しました");
}

//=============================================================================
// タイムの文字
//=============================================================================
std::string FormatTime(float seconds) {
	// 1/100 秒の整数にしてから分ける（float のまま割ると、59.999 が 1:00.00 になるなど、切り上がることがある）
	const long long centiseconds = static_cast<long long>(std::floor((std::max)(seconds, 0.0f) * 100.0f));
	const long long minutes = centiseconds / 6000;
	const long long secondsPart = (centiseconds / 100) % 60;
	const long long hundredths = centiseconds % 100;
	return std::format("{}:{:02}.{:02}", minutes, secondsPart, hundredths);
}
```

### 解説

**ファイルの中身**
```json
{
    "stages": {
        "Level01": { "bestTime": 30.0, "cleared": true },
        "Level02": { "cleared": false }
    },
    "version": 1
}
```
- **番号ではなくシーンの名前で書く**。一覧を並べ替えても、ステージを足しても、記録は名前で探すのでずれない。
- 一覧から消したステージの記録も、**読んだ JSON（`state.file`）を丸ごと覚えておいて書き戻す**ので消えない（また一覧に戻せば、記録も戻る）。
- `ResetAll` だけは、それも含めて全部消す。

**中身を .cpp の無名の namespace に置いた理由**
- ヘッダに `nlohmann::json` を書くと、GameProgress.h を読む全部の .cpp が、とても大きい json.hpp を読むことになり、ビルドが遅くなる。ヘッダは関数だけにして、中身（`ProgressState`）は .cpp の中だけに置いた。
- 無名の namespace（`namespace { … }`）の中の物は、その .cpp の外から見えない（ほかの .cpp の同じ名前とぶつからない）。

**壊れたファイルでも落ちない**
- `nlohmann::json::parse(stream, nullptr, false)`：3つ目の `false` で、読めないときに例外を投げず、`is_discarded()` が true の値を返す（エンジンの `SceneManager::GetLastOpenedScene` と同じ書き方）。
- 読めない・無い → 何もクリアしていない扱い（ゲームは始められる）。

**`std::optional<float>`**：「タイムがある・無い」を1つの値で持つ。`if (best)` で「ある？」、`*best` で中身。まだクリアしていないのに 0 秒などの仮の数を入れると、「0 秒が1位」と区別がつかなくなる。

**`FormatTime` で、先に 1/100 秒の整数にする理由**
- 32.456 秒 → `floor(32.456 × 100)` = 3245（1/100 秒）→ 分 0・秒 32・1/100 秒 45 → `"0:32.45"`（切り捨て）。
- 秒を float のまま `fmod` などで分けると、59.999 のような値で「秒が 60」になったり、表示が切り上がったりすることがある。整数にしてから割れば、ずれない。

---

## パート D：`Source/Scripts/UI/MenuBase.h / .cpp`（少し直す）

ステージ選択のマスは、コードで作る（パート H）。**Entity はすぐできるが、Text が付くのは次のフレームの頭**（エンジンの `AddComponent` の決まり）。今の MenuBase は最初のフレームに「元の飾り」を覚えるので、まだ Text の無いマスは「飾り無し（色が 0＝見えない）」を覚えてしまう。**Text が付いた時に覚える**ように直す。

### `MenuBase.h`

① 一番上の include に `<optional>` を足す：
```cpp
#pragma once
#include <optional>
#include <string>
#include <vector>
```

② `SetColumns` の下に1行足す（ステージ選択がマスを並べるのに、Columns を読む）：
```cpp
	// 横に並べる数（1：縦の並び。2 以上：格子）
	void SetColumns(int columns) { columns_ = columns; }
	int GetColumns() const { return columns_; }
```

③ 一番下の `originalStyles_` の型を変える：
```cpp
	// 前
	std::vector<TextStyle> originalStyles_; // 項目の元の飾り（最初のフレームに覚える。外れた項目をこれに戻す）
	// 後
	std::vector<std::optional<TextStyle>> originalStyles_; // 項目の元の飾り（Text が付いている項目の分だけ覚える。外れた項目をこれに戻す）
```

### `MenuBase.cpp`：`ApplyLook` を全部差し替え

前：
```cpp
void MenuBase::ApplyLook(const std::vector<Handle<Entity>>& items) {
	// --- 項目の元の飾りを覚える（最初のフレームと、項目の数が変わったとき。外れた項目を戻すのに使う）---
	if (originalStyles_.size() != items.size()) {
		originalStyles_.clear();
		for (const Handle<Entity> item : items) {
			const TextComponent* text = EntityManager::Get<TextComponent>(item);
			originalStyles_.push_back(text != nullptr ? TextStyle::From(*text) : TextStyle{});
		}
	}
	// --- 見本の飾り（見本が無ければ、選んだ項目も元のまま）---
	const GameObject styleObject = gameObject.FindChild(styleName_);
	const TextComponent* styleText = styleObject ? styleObject.GetComponent<TextComponent>() : nullptr;
	for (size_t i = 0; i < items.size(); ++i) {
		TextComponent* text = EntityManager::Get<TextComponent>(items[i]);
		if (text == nullptr) {
			continue; // Text の無い項目（格子のマスなど）は、派生クラスが OnSelect で見せる
		}
		const bool isSelected = (static_cast<int>(i) == selected_);
		if (isSelected && styleText != nullptr) {
			TextStyle::From(*styleText).ApplyTo(*text);
		} else {
			originalStyles_[i].ApplyTo(*text);
		}
	}
}
```

後：
```cpp
void MenuBase::ApplyLook(const std::vector<Handle<Entity>>& items) {
	// --- 項目の数が変わったら、覚えた飾りを忘れる ---
	if (originalStyles_.size() != items.size()) {
		originalStyles_.assign(items.size(), std::nullopt);
	}
	// --- 項目の元の飾りを覚える（Text が付いた最初のフレームに。外れた項目を戻すのに使う）---
	// コードで作った項目（ステージ選択のマス）は、Text が付くのが次のフレームなので、付くまで待つ
	for (size_t i = 0; i < items.size(); ++i) {
		const TextComponent* text = EntityManager::Get<TextComponent>(items[i]);
		if (!originalStyles_[i] && text != nullptr) {
			originalStyles_[i] = TextStyle::From(*text);
		}
	}
	// --- 見本の飾り（見本が無ければ、選んだ項目も元のまま）---
	const GameObject styleObject = gameObject.FindChild(styleName_);
	const TextComponent* styleText = styleObject ? styleObject.GetComponent<TextComponent>() : nullptr;
	for (size_t i = 0; i < items.size(); ++i) {
		TextComponent* text = EntityManager::Get<TextComponent>(items[i]);
		if (text == nullptr || !originalStyles_[i]) {
			continue; // Text の無い（まだ付いていない）項目は何もしない
		}
		const bool isSelected = (static_cast<int>(i) == selected_);
		if (isSelected && styleText != nullptr) {
			TextStyle::From(*styleText).ApplyTo(*text);
		} else {
			originalStyles_[i]->ApplyTo(*text);
		}
	}
}
```

- `std::optional<TextStyle>` の空（`std::nullopt`）＝「まだ覚えていない」。Text が付いたフレームに初めて覚える。
- 覚えていない項目は飾りを書かない（何もしない）。次のフレームに Text が付けば、普通に動く。
- 今までの項目（Text が最初から付いている）は、最初のフレームで覚えるので、動きは前と同じ。

---

## パート E：`Source/Scripts/LevelInfo.h / .cpp`（全部差し替え）

### `LevelInfo.h`

```cpp
#pragma once
#include <cstddef>
#include <optional>
#include <string>

#include <MyEngine/ECS/Script.h>


/// <summary>
/// レベル（1つのシーン）の情報と進み方。シーンに1つ、空の Entity（名前は Level）に付ける
/// <para>番号（ステージの一覧 kStages の何番目か。1 から）・場所の名前・下の線の高さ（これより下に落ちたら一発アウト）を持つ</para>
/// <para>遊んだ時間を数え、ゴールした（Clear）ら止めて、進み具合（GameProgress）に記録する。次のステージは一覧の次</para>
/// <para>R キー / パッドの Back で、いつでもレベルの最初からやり直せる。Esc / パッドの Start でポーズ画面（Pause Screen）を出す（クリアした後はどちらも効かない）</para>
/// </summary>
class LevelInfo : public Script {
public:
	void Start() override;
	void Update() override;
	void Fields(ComponentUI& ui) override;

	// ===== ほかの Script から使う =====
	int GetNumber() const { return number_; }
	// ステージの一覧（kStages）の番号（0 から）
	size_t GetStageIndex() const { return static_cast<size_t>(number_ > 0 ? number_ - 1 : 0); }
	const std::string& GetAreaName() const { return areaName_; }
	float GetKillHeight() const { return killHeight_; }
	// 遊んだ時間（秒。ポーズ中は数えない。クリアしたら止まる）
	float GetPlayTime() const { return playTime_; }

	// クリアした（GoalDoor から）。時間を止めて記録する。2回目からは何もしない
	void Clear();
	bool IsCleared() const { return isCleared_; }
	// Clear した時の、前の1位のタイム（初めてのクリアなら空）
	std::optional<float> GetPreviousBest() const { return previousBest_; }
	// Clear した時のタイムが、新しい1位か（初めてのクリアも 1位）
	bool IsNewBest() const { return !previousBest_ || playTime_ < *previousBest_; }

	// 次のステージがあるか（一覧の最後でない・シーンがある）
	bool HasNextLevel() const;
	// 次のステージへ（幕）。無ければタイトルへ
	void LoadNextLevel() const;
	// 今のシーンの LevelInfo（無ければ nullptr）。ポインタは使い捨て（フレームをまたいで持たない）
	static LevelInfo* Find() { return FindObjectOfType<LevelInfo>(); }

private:
	// ===== Inspector に出す・保存する値 =====
	int number_ = 1;                    // ステージの番号（kStages の何番目か。1 から）
	std::string areaName_ = "森";       // 場所の名前
	float killHeight_ = -10.0f;         // これより下に落ちたら一発アウト（ワールドの Y）
	std::string pauseScreen_ = "Pause"; // Esc で出す画面の名前（resources/ui/<名前>.uiscreen。空ならポーズしない）
	std::string titleScene_ = "Title";  // 最後のステージの次に読むシーンの名前

	// ===== 実行中だけの値 =====
	float playTime_ = 0.0f;             // 遊んだ時間（秒）
	bool isCleared_ = false;            // クリアした（時間を止める・Esc と R を止める）
	std::optional<float> previousBest_; // Clear した時の、前の1位のタイム
};
```

### `LevelInfo.cpp`

```cpp
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

void LevelInfo::LoadNextLevel() const {
	// 次が無ければ（最後のステージ・まだ作っていない）タイトルへ
	const std::string scene = HasNextLevel() ? std::string(kStages[GetStageIndex() + 1].scene) : titleScene_;
	SceneTransition::LoadScene(scene);
}
```

### 変わった所
| 前 | 後 |
|---|---|
| Next Scene（シーンごとに次の名前を書く） | **やめた**。次は一覧（kStages）の次。`Number` が一覧の何番目か |
| 次が無い・名前が違う → 同じレベルをもう一度 | 次が無い（最後・まだ作っていない）→ **タイトルへ** |
| `SceneManager::LoadScene`（幕なし） | **`SceneTransition::LoadScene`**（幕） |
| — | **遊んだ時間**を数える（`Time::GetDeltaTime`。ポーズ中は Update が呼ばれないので数えない） |
| — | **`Clear()`**：時間を止めて、`GameProgress::RecordClear` に記録。前の1位を覚える（段階3 のクリア画面で「New Best」を出すのに使う） |
| R・Esc はいつでも | **クリアした後は効かない**（扉に吸い込まれている間に、やり直し・ポーズが開かないように） |

- `IsNewBest()`：前の1位が無い（初めてのクリア）か、今のタイムの方が速い。
- **時間は、シーンを開いた瞬間（幕が開く間も）から数える**。幕の間を数えたくなければ、`Update` の頭で `if (SceneTransition::IsBusy()) return;` を足す（全員に同じだけ足されるので、今回は気にしない）。
- `Start` で `gOutGame.lastStage` に今のステージを入れる：ポーズの「Title」でタイトルへ戻ったとき、ステージ選択で今のステージが選ばれている。クリアしたら次のステージ。

---

## パート F：`Source/Scripts/GoalDoor.cpp`（1か所足す）

`OnTriggerEnter` の最後に3行足す。

前：
```cpp
void GoalDoor::OnTriggerEnter(GameObject other) {
	// --- 早期リターン：キューブでない・もうやられた・もうクリアした ---
	IceCube* cube = other.GetComponent<IceCube>();
	if (cube == nullptr || !cube->IsPlayable() || isCleared_) {
		return;
	}
	// --- クリア：キューブを扉へ吸い込ませて、時間を数え始める ---
	cube->Clear(GetWorldPosition(transform));
	isCleared_ = true;
}
```

後：
```cpp
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
}
```

- ゴールの流れ（Delay 秒たったら `LoadNextLevel`）は、段階3 でクリア画面に替えるまで今のまま。
- `#include "LevelInfo.h"` は、GoalDoor.cpp にもうある。

**ここで一度ビルドして、Level01 をゴールする**。`project/saves/Progress.json` ができて、`"Level01": {"bestTime": …, "cleared": true}` が入っていればよい。

---

## パート G：`Source/Scripts/UI/TitleMenu.h / .cpp`（全部差し替え）

### `TitleMenu.h`

```cpp
#pragma once
#include <string>

#include "Source/Scripts/UI/MenuBase.h"

/// <summary>
/// タイトル画面のメニュー。Title シーンの TitleUI（空の Entity）に付ける
/// <para>項目の名前で何をするか決める：Start（ステージ選択の画面を出す。シーンは替えない）/ Quit（ゲームを終わる）。選ぶ・飾りは MenuBase</para>
/// <para>子の Logo（Text）があれば、ゲームの名前（GameTitle.h の kGameTitle）を書く</para>
/// <para>ステージ選択の画面が出ている間は、タイトルの項目は動かず、タイトルの文字（この Entity と子）は静かに消える（Alpha Group）</para>
/// <para>エディタだけ：F1 で全部のステージを開く・F2 で記録を全部消す</para>
/// </summary>
class TitleMenu : public MenuBase {
public:
	void Fields(ComponentUI& ui) override;

protected:
	void OnStart() override;
	void OnUpdate() override; // タイトルの文字を消す・戻す。デバッグのキー
	void Decide(int index, const std::string& itemName) override;
	bool IsInputBlocked() const override; // ステージ選択の画面が出ている間

private:
	// ゲームを終わる（エディタでは終わらない）
	static void QuitGame();

	// ===== Inspector に出す・保存する値 =====
	std::string stageSelectScreen_ = "StageSelect"; // Start で出す画面の名前（resources/ui/<名前>.uiscreen）
	std::string logoName_ = "Logo";                 // ゲームの名前を書く Text の名前（一番上の直下の子。無ければ書かない）
	float fadeTime_ = 0.5f;                         // ステージ選択を出した・閉じたとき、タイトルの文字が消える・戻るまでの秒数
};
```

### `TitleMenu.cpp`

```cpp
#include "TitleMenu.h"

#include <algorithm>

#include <MyEngine/Components/AlphaGroupComponent.h>
#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/Engine.h>
#include <MyEngine/Input/InputManager.h>

#include "Source/Scripts/GameTitle.h"
#include "Source/Scripts/Progress/GameProgress.h"

SCRIPT(TitleMenu, "UI");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void TitleMenu::Fields(ComponentUI& ui) {
	MenuBase::Fields(ui);
	ui.Label("項目の名前で何をするか決める：Start（ステージ選択の画面を出す）/ Quit（ゲームを終わる。エディタでは終わらない）");
	ui.Field("Stage Select Screen", stageSelectScreen_, Tip("Start で出す画面の名前（resources/ui の .uiscreen の名前）"));
	ui.Field("Logo Name", logoName_, Tip("ゲームの名前（GameTitle.h の kGameTitle）を書く Text の名前（一番上の直下の子）。無ければ書かない"));
	ui.Field("Fade Time", fadeTime_, AtLeast(0.01f).Tip("ステージ選択を出した・閉じたとき、タイトルの文字（この Entity と子）が消える・戻るまでの秒数"));
	ui.Label("エディタだけ：F1 で全部のステージを開く・F2 で記録を全部消す（ステージ選択を開く前に押す）");
}

//=============================================================================
// 最初・毎フレーム
//=============================================================================
void TitleMenu::OnStart() {
	// --- ロゴにゲームの名前を書く（名前は GameTitle.h の1か所だけで変える）---
	if (const GameObject logo = gameObject.FindChild(logoName_)) {
		if (TextComponent* text = logo.GetComponent<TextComponent>()) {
			text->text = kGameTitle;
		}
	}
	// --- タイトルの文字をまとめて消すための Alpha Group（無ければ付ける。付くのは次のフレーム）---
	if (GetComponent<AlphaGroupComponent>() == nullptr) {
		gameObject.AddComponent(AlphaGroupComponent{});
	}
}

void TitleMenu::OnUpdate() {
	// --- ステージ選択が出ている間は消す・閉じたら戻す（止めない時間で、一定の速さで近づける）---
	if (AlphaGroupComponent* group = GetComponent<AlphaGroupComponent>()) {
		const float target = UIScreen::IsOpen(stageSelectScreen_) ? 0.0f : 1.0f;
		const float step = Time::GetUnscaleDeltaTime() / fadeTime_;
		group->alpha = (group->alpha < target) ? (std::min)(group->alpha + step, target) : (std::max)(group->alpha - step, target);
	}
#ifdef USE_IMGUI
	// --- デバッグ：進み具合を変える（ステージ選択は開くたびにマスを作るので、開く前に押す）---
	if (InputManager::IsKeyTriggered(DIK_F1)) {
		GameProgress::UnlockAll();
	}
	if (InputManager::IsKeyTriggered(DIK_F2)) {
		GameProgress::ResetAll();
	}
#endif
}

//=============================================================================
// 決める
//=============================================================================
void TitleMenu::Decide(int /*index*/, const std::string& itemName) {
	if (itemName == "Start") {
		// 画面を重ねるだけ（シーンは替えない。背景はそのまま）。ファイルが無ければ出ない
		if (!UIScreen::Open(stageSelectScreen_)) {
			LogManager::Warning("ステージ選択の画面を出せませんでした（resources/ui/" + stageSelectScreen_ + ".uiscreen が無い？）");
		}
	} else if (itemName == "Quit") {
		QuitGame();
		isDecided_ = true;
	}
}

bool TitleMenu::IsInputBlocked() const { return UIScreen::IsOpen(stageSelectScreen_); }

//=============================================================================
// 終わる
//=============================================================================
void TitleMenu::QuitGame() {
#ifdef USE_IMGUI
	// エディタ：ウィンドウを閉じるとエディタごと終わるので、知らせるだけ（Play を止めるのは手で）
	LogManager::Log("Quit：エディタでは終了しません（製品版ではここでゲームが終わる）");
#else
	// 製品版：自分のウィンドウに「閉じて」を置く → 次の ProcessMessage でエンジンの WM_CLOSE の処理が DestroyWindow → main のループが終わる
	// PostMessage は置くだけなので、この Script の途中でウィンドウが消えることはない
	PostMessage(Engine::GetWindowManager()->GetMainHWND(), WM_CLOSE, 0, 0);
#endif
}
```

### 変わった所
- **ステージ選択が出ている間、タイトルの文字（ロゴ・Start・Quit）を静かに消す**。TitleUI に Alpha Group（子の Text をまとめて透明にする Component）を付けて、その Alpha を 0.5 秒で 0 へ／1 へ動かす。
  - Alpha Group が無ければ `OnStart` でコードが付ける（付くのは次のフレーム。それまでは `GetComponent` が nullptr なので何もしない）。エディタで付けておいてもよい。
  - 「一定の速さで近づける」：`step = 経過秒 ÷ Fade Time` ずつ、目標（0 か 1）へ近づけて、目標を越えないように `min`・`max` で止める。途中で開く・閉じるが変わっても、その場から向きを変えるだけなので、ぎくしゃくしない。
  - 時間は `GetUnscaleDeltaTime`（止めない時間）。
- **F1・F2（エディタだけ）**：全部のステージを開く・記録を全部消す。ステージ選択はマスを開くたびに作るので、**開く前に**押す。
- `std::min`・`std::max` は `(std::min)(…)` と括弧で囲む：Windows.h の `min`・`max` のマクロと名前がぶつかるのを避ける（CLAUDE.md の決まり）。

---

## パート H：`Source/Scripts/UI/StageSelectMenu.h / .cpp`（新しいファイル）

### `StageSelectMenu.h`

```cpp
#pragma once
#include <string>

#include <MyEngine/Math/Vector2.h>
#include <MyEngine/Math/Vector4.h>

#include "Source/Scripts/UI/MenuBase.h"

/// <summary>
/// ステージ選択の画面。resources/ui/StageSelect.uiscreen の一番上の Entity に付ける（タイトルの上に重ねる。シーンは替えない）
/// <para>マスは、ステージの一覧（StageCatalog.h の kStages）の数だけ、見本の Text（Cell Template）を写して子の Items の下に作る。
/// 一覧に行を足せば、マスも増える</para>
/// <para>下の2行（Stage Name・Best Time の Text）に、選んでいるステージの名前と1位のタイムを出す</para>
/// <para>遊べるのは、1つ前をクリアしたステージだけ。決めたら、そのシーンへ（幕）。戻るキーで閉じる</para>
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

private:
	// マスを作る（見本の Text を写して、文字・色・位置だけ変える）
	void CreateCells();
	// 子の name の Text に文字を書く（無ければ何もしない）
	void SetText(const std::string& name, const std::string& text) const;

	// ===== Inspector に出す・保存する値 =====
	std::string screenName_ = "StageSelect";      // この画面の名前（閉じるときに UIScreen::Close に渡す）
	std::string templateName_ = "CellTemplate";   // マスの見本の Text（一番上の直下の子。Text の Enabled を外して置く。この位置が1つ目のマス）
	Vector2 cellSpacing_ = {120.0f, 96.0f};       // マスとマスの間（ピクセル。横・縦。2段目は下へ）
	Vector4 clearedColor_ = {1.0f, 1.0f, 1.0f, 1.0f};  // クリアしたマスの色
	Vector4 openColor_ = {1.0f, 1.0f, 1.0f, 0.6f};     // 遊べるがまだクリアしていないマスの色
	Vector4 lockedColor_ = {1.0f, 1.0f, 1.0f, 0.18f};  // まだ遊べないマスの色
	std::string stageNameName_ = "StageName";     // ステージの名前を書く Text（一番上の直下の子）
	std::string bestTimeName_ = "BestTime";       // 1位のタイムを書く Text（一番上の直下の子）
	std::string bestLabel_ = "Best";              // タイムの前に付ける言葉
	std::string noRecordText_ = "--:--.--";       // まだクリアしていないときのタイム
	std::string lockedText_ = "Locked";           // まだ遊べないときに、タイムの代わりに出す言葉
	std::string comingText_ = "Coming soon";      // シーンがまだ無いときに、タイムの代わりに出す言葉
	std::string lockedHint_ = "Clear the previous stage"; // まだ遊べないマスを決めたときに、しばらく出す言葉
	float hintTime_ = 1.5f;                       // 上の言葉を出しておく秒数

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
	const size_t stage = static_cast<size_t>(index);
	if (stage >= kStageCount) {
		return;
	}
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
	const size_t stage = static_cast<size_t>(index);
	gOutGame.lastStage = stage;
	isDecided_ = SceneTransition::LoadScene(std::string(kStages[stage].scene));
}

void StageSelectMenu::OnCancel() {
	UIScreen::Close(screenName_); // 閉じると、タイトルの項目がまた動く
	isDecided_ = true;
}

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

### 解説

**マスを作る（`CreateCells`）**
1. 見本の Text（Cell Template）の `TextComponent` を**丸ごと写す**（`TextComponent text = *templateText;`）。Component は memcpy できる型（`static_assert` で決まっている）なので、普通の代入で全部の値が写る。フォント・大きさ・飾りは見本のまま。
2. 文字（`"01"`）・色（クリアした／遊べる／まだ）・Enabled だけ変える。`std::format("{:02}", 1)` は「2桁に足りなければ 0 を前に足す」→ `"01"`。
3. `GameObject::Create(名前, items)` で Items の子に Entity を作る（Transform はすぐ付いている）。位置は見本の位置から、列 × Cell Spacing.x だけ右、段 × Cell Spacing.y だけ下。
4. `AddComponent(text)` で Text を付ける（次のフレームの頭に付く。だからパート D の直しが要る）。
- 列と段：`i % columns`（余り＝何列目）、`i / columns`（割り算の答え＝何段目）。5列なら 0〜4 が1段目、5〜9 が2段目。
- マスは画面の中の物（UI Screen の一番上の Entity の子孫）なので、画面を閉じると一緒に消える。開くたびに作り直すので、F1・F2 の結果もそこで反映される。

**最初に選ぶマス**：`gOutGame.lastStage`（最後に遊んだ／次に遊ぶステージ）。それがまだ遊べなければ、1つずつ前へ戻って、遊べる所で止まる（0 番はいつも遊べる）。

**遊べないマスを決めたとき**：震わせたり音で怒ったりせず、タイムの行をしばらく「Clear the previous stage」にして、Hint Time 秒たったら元の言葉に戻す（`OnUpdate`）。別のマスへ動いたら、すぐ戻る（`OnSelect` の頭で `hintTimer_ = 0`）。

**`HasStageScene`**：`SceneManager::HasScene` で、シーンのファイルがあるか調べる。一覧に書いたが、まだ作っていないステージ（Level03〜）は入れない。

---

## パート I：VS のプロジェクト・.gitignore

| フォルダ（フィルター） | 入れるファイル |
|---|---|
| Source/Scripts/Progress（新しく作る） | `StageCatalog.h`・`OutGameState.h`・`GameProgress.h`・`GameProgress.cpp` |
| Source/Scripts/UI | `StageSelectMenu.h`・`StageSelectMenu.cpp` |

`.gitignore`（AL4_01 のルート）の一番下に足す（遊んだ記録は、人によって違うので git に入れない）：
```gitignore

# 遊んだ記録（GameProgress）
/project/saves/
```

---

## パート J：エディタ

### J-1. Level01・Level02 を保存し直す
LevelInfo から Next Scene を外したので、開くと Console に「ファイルの項目 "Next Scene" を読む所がありません」が出る。**開いて Ctrl+S で保存し直せば消える**（そのままでも動く）。
- Level 02 の LevelInfo の Number が 2 になっているか見る（Level01 は 1）。

### J-2. StageSelect.uiscreen を作る
1. Project ウィンドウの `resources/ui` で **Create → UI Screen** → 名前 `StageSelect`。
2. 開いて（Edit UI Screen）、下のように作る。**名前はコードが探すので、このとおりに**。

| Entity | 親 | Component・値 |
|---|---|---|
| **StageSelect**（一番上） | — | UI Screen：**Pause Game を外す**・Show：Fade（Duration 0.5・OutSine）・Show Stagger 0.06・Hide：Fade 0.3。Add Component → Gameplay → UI → **Stage Select Menu**：**Columns 5**・Hit Padding 16 |
| Header | StageSelect | Text「Select Stage」・Font Size 28・Color A 0.7・位置 (0, 200) |
| **CellTemplate** | StageSelect | Text「00」・Font Size 48・位置 (-240, 80)・**Text の Enabled を外す**（見本。画面には出ない。この位置が 01 のマス） |
| **Items** | StageSelect | 空の Entity。**位置は (0, 0) のまま**（マスの位置は Cell Template から数えるので、ずらすとマスもずれる） |
| **StageName** | StageSelect | Text・Font Size 32・位置 (0, -150) |
| **BestTime** | StageSelect | Text・Font Size 22・Color A 0.6・位置 (0, -195) |
| **SelectedStyle** | StageSelect | Text・**Enabled を外す**・Color 白・Glow 10・Glow Color (0.75, 0.93, 1.0, 0.55)。ほかは無し（縁取り・Shine・Motion を付けない） |

- フォントは Title と同じ Noto Sans JP（全部の Text）。
- 位置・大きさ・色は好みで。Title が左寄せなので、ステージ選択も左寄せにするなら、Cell Template・StageName・BestTime を左へ（Text の Align も Left に）。マスの間は Stage Select Menu の **Cell Spacing**（初めは 120 × 96）。
- Stage Select Menu の Cleared / Open / Locked Color で、マスの明るさの差を決める（初めは 1.0・0.6・0.18）。
- 作業用コピーで、この値の画面（OutGame.md 5.1 の図の形）が出るのを確かめた。

### J-3. Title の SelectedStyle（見直し）
段階1 の md で「Bounce で跳ねる」を勧めたが、世界観に合わないのでやめる：Motion は None、Shine は外す、Glow を少しだけ（10・氷色・A 0.5 くらい）。

---

## 確かめること（写した後）

| 何をする | こうなればよい |
|---|---|
| Level01 を Play してゴール | `project/saves/Progress.json` に Level01 の cleared と bestTime。次は Level02（幕） |
| Level02 をゴール | 次のステージのシーンが無いので、Title へ（幕） |
| Title で Start | タイトルの文字が静かに消えて、ステージ選択が出る。01 が選ばれていて、下に「Stage 01」「Best  0:32.45」のように出る |
| ←→↑↓・マウス | マスを選べる。下の2行が変わる。遊べないマスは「Locked」、シーンが無いマスは「Coming soon」 |
| 遊べないマスで決める | タイムの行が 1.5 秒「Clear the previous stage」になって戻る |
| 遊べるマスで決める | そのステージへ（幕） |
| Esc・右クリック | ステージ選択が消えて、タイトルの文字が戻る。Start・Quit がまた選べる |
| Title で F2 → Start | 01 以外は全部遊べない（Locked）・タイムは --:--.-- |
| Title で F1 → Start | 全部遊べる（シーンが無いマスは Coming soon） |
| ポーズの Title で戻る → Start | さっき遊んでいたステージが選ばれている |
| R・Esc をゴールの後に押す | 何も起きない |

## 作業用コピーで確かめたこと（2026-10-26）
- 段階1 を写した今の AL4_01（名前「森の中の氷」・ユーザーの Title シーン）を写して、このページのコードを入れた。**Debug・Release ともビルドが通った**（エラー 0。ゲームのファイルから警告 0）。Debug（エディタ）も起動した（出た警告は J-1 の Next Scene だけ）。
- J-2 と同じ値の StageSelect.uiscreen を作業用コピーに作り、`saves/Progress.json` に Level01 のクリア（32.456 秒）を入れて、一時的に Title から自動で開いた：**マスが 01〜10 の 5×2 で並び、01（クリア）は白、02（遊べる）は少し薄く、03〜10 はほとんど見えない。01 が選ばれてうっすら光り、下に「Stage 01」「Best  0:32.45」**。タイトルの文字（ロゴ・Start・Quit）は消えていた。
- 一時的に `RecordClear(0, 40)`・`RecordClear(0, 30)`・`RecordClear(1, 51.234)` を呼んで、ファイルを見た：40 秒では 1位が変わらず、30 秒で 30 に変わり、Level02 が増えた。**一覧に無い「OldStage」の記録も消えずに残った**。
- キー・マウスで実際に選ぶ・ゴールする所は試していない（ユーザーの PC に本物の入力は送らないため）。上の表で確かめてほしい。

## 困ったとき

| 症状 | 原因 |
|---|---|
| `GameProgress::…` が未解決（リンクエラー） | GameProgress.cpp をプロジェクトに入れていない |
| `gOutGame` が二重定義 | OutGameState.h の `inline` を書き忘れた |
| マスが出ない | Cell Template・Items の名前が違う（Console に「子の Items か、Cell Template（Text）がありません」）・Items が StageSelect の直下でない |
| マスが全部同じ所に重なる | Columns が 1 のまま（縦に並ぶ）・Cell Spacing が 0 |
| マスが見えない・選んでも光らない | パート D（MenuBase の直し）を写していない（Text が付く前の「飾り無し」を覚えてしまう） |
| 下の2行が変わらない | StageName・BestTime の名前が違う・StageSelect の直下でない |
| タイトルの文字が消えない | TitleMenu を写し直していない・Alpha Group の Enabled が外れている |
| Progress.json ができない | 作業フォルダ（起動したフォルダ）に書けない・GoalDoor のパート F を写していない |
