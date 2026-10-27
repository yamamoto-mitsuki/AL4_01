#pragma once
#include <string>
#include <cstddef>
#include <optional>
#include <vector>


/// <summary>
/// 進み具合（どのステージをクリアしたか・1位のタイム・クリアした回数・取った隠し結晶）。saves/Progress.json に保存する
/// <para>初めて読まれた時に1回ファイルを読み、変わったらすぐ書く。番号は kStages（StageCatalog.h）の番号</para>
/// <para>ファイルにはシーンの名前で書くので、kStages を並べ替える・足す・消すしても記録はずれない</para>
/// </summary>
class GameProgress {
public:
	// クリアしたか
	static bool IsCleared(size_t index);
	// 遊べるか（今は全部いつも。GameProgress.cpp の kAllStagesOpen を false にすると、0 番はいつも・ほかは1つ前をクリアしたら）
	static bool IsUnlocked(size_t index);
	// 1位のタイム（秒）。まだクリアしていなければ空
	static std::optional<float> GetBestTime(size_t index);
	// クリアした回数（まだクリアしていなければ 0）
	static int GetClearCount(size_t index);

	/// <summary>
	/// クリアした。クリアした回数を1つ増やし、タイムが1位なら覚える。すぐファイルに書く
	/// </summary>
	/// <returns>前の1位のタイム（初めてのクリアなら空）。新しい1位か調べるのに使う</returns>
	static std::optional<float> RecordClear(size_t index, float time);

	// ===== 隠し結晶（SecretCrystal）=====
	// 取ったか（id は SecretCrystal の Id。ステージの中で見分ける番号）
	static bool IsCrystalCollected(size_t index, int id);
	// 取った（ゴールした時に LevelInfo から。ids は今回取った Id）。初めて取った物があれば、ファイルに書く
	static void RecordCrystals(size_t index, const std::vector<int>& ids);
	// 取った数（ステージに置いてある数を越えない。結晶を減らしたときに 4/3 にならないように）
	static int GetCrystalCount(size_t index);
	// ステージに置いてある数（一度そのステージを遊ぶと分かる。まだなら 0）
	static int GetCrystalTotal(size_t index);
	// ステージに置いてある数を覚える（LevelInfo がステージの始まりに数えて渡す）。変わったときだけファイルに書く
	static void SetCrystalTotal(size_t index, int total);

	// ===== 全部を変える =====
	static void UnlockAll(); // 全部クリアした扱いにする（タイム・回数は変えない）。デバッグ（エディタのタイトルで F1）
	static void ResetAll();  // 全部消す。設定画面の Reset Data・デバッグ（エディタのタイトルで F2）
};

// 秒を「分:秒.1/100秒」にする（32.456 → "0:32.45"。1/100 秒より下は切り捨て）
std::string FormatTime(float seconds);