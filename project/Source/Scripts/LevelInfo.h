#pragma once
#include <string>
#include <cstddef>
#include <optional>
#include <vector>

#include <MyEngine/ECS/Script.h>


/// <summary>
/// レベル（1つのシーン）の情報と進み方。シーンに1つ、空の Entity（名前は Level）に付ける
/// <para>番号（ステージの一覧 kStages の何番目か。1 から）・場所の名前・下の線の高さ（これより下に落ちたら一発アウト）を持つ</para>
/// <para>遊んだ時間を数え、ゴールした（Clear）ら止めて、進み具合（GameProgress）に記録する。次のステージは一覧の次</para>
/// <para>R キー / パッドの Back で、いつでもやり直せる（中間地点 Checkpoint を取っていたらそこから。時間は続きから）。Esc / パッドの Start でポーズ画面（Pause Screen）を出す（クリアした後はどちらも効かない）</para>
/// <para>中間地点の記録（gCheckpoint）を、ほかの入り方（ポーズの Restart・ステージ選択・次のステージ）のときに Start で消す</para>
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
	// ステージの始まりの演出（ぼかし＋ステージの名前）の途中か。開いた直後から、ぼかしが戻りきるまで
	bool IsIntroPlaying() const;

	// やり直す（やられた・R）。幕を閉じて読み直し、中間地点を取っていたらそこから・時間は続きから。幕を始められたら true
	bool Retry(float fadeTime) const;
	// クリアした（GoalDoor から）。時間を止めて記録する。2回目からは何もしない
	void Clear();
	bool IsCleared() const { return isCleared_; }
	// 隠し結晶を取った（SecretCrystal から）。ゴールした時（Clear）に保存する。やられて読み直すと、この記録は消える（取っていない扱い）
	// ただし中間地点に触れるまでに取った物は、中間地点から復活しても取った扱いのまま（KeepCrystals）
	void CollectCrystal(int id) { collectedCrystals_.push_back(id); }
	// 中間地点に触れた（Checkpoint から）。今までに取った隠し結晶を、中間地点に覚えてもらう（復活しても取り直さなくてよい）
	void KeepCrystals() const;
	// 今回取った隠し結晶の Id（中間地点から持ち越した物も入る）
	const std::vector<int>& GetCollectedCrystals() const { return collectedCrystals_; }
	// Clear した時の、前の1位のタイム（初めてのクリアなら空）
	std::optional<float> GetPreviousBest() const { return previousBest_; }
	// Clear した時のタイムが、新しい1位か（初めてのクリアも 1位）
	bool IsNewBest() const { return !previousBest_ || playTime_ < *previousBest_; }

	// 次のステージがあるか（一覧の最後でない・シーンがある）
	bool HasNextLevel() const;
	// 次のステージへ（幕）。無ければタイトルへ。幕を始められたら true（もう幕の途中なら false）
	bool LoadNextLevel() const;
	// タイトルへ（幕。Title Scene のシーン）。幕を始められたら true
	bool LoadTitle() const;
	// 今のシーンの LevelInfo（無ければ nullptr）。ポインタは使い捨て（フレームをまたいで持たない）
	static LevelInfo* Find() { return FindObjectOfType<LevelInfo>(); }

private:
	// 置いてある隠し結晶（SecretCrystal）を数えて、進み具合に覚える（同じ Id があれば知らせる）
	void CountCrystals() const;

	// ===== Inspector に出す・保存する値 =====
	int number_ = 1;                   // ステージの番号（kStages の何番目か。1 から）
	std::string areaName_ = "森";       // 場所の名前
	float killHeight_ = -10.0f;         // これより下に落ちたら一発アウト（ワールドの Y）
	std::string pauseScreen_ = "Pause"; // Esc で出す画面の名前（resources/ui/<名前>.uiscreen。空ならポーズしない）
	std::string introScreen_ = "StageIntro"; // ステージの始まりに出す画面の名前（空なら出さない）
	std::string titleScene_ = "Title";  // 最後のステージの次に読むシーンの名前

	// ===== 実行中だけの値 =====
	float playTime_ = 0.0f;             // 遊んだ時間（秒）
	bool hasUpdated_ = false;           // 1回目の Update が済んだ（中間地点から始める読み直しの印を、最初のフレームだけで下ろす）
	bool isCleared_ = false;            // クリアした（時間を止める・Esc と R を止める）
	std::optional<float> previousBest_; // Clear した時の、前の1位のタイム
	std::vector<int> collectedCrystals_; // 今回取った隠し結晶の Id（ゴールしたら保存。読み直すと、中間地点に覚えてもらった物だけに戻る）
};