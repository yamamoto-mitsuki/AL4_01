#pragma once
#include <string>
#include <cstddef>
#include <optional>


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