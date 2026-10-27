#pragma once
#include <cstdint>


/// <summary>
/// 音の大きさのつまみ（設定画面の項目の名前・saves/Settings.json の項目名と同じ）
/// </summary>
enum class VolumeKind : uint8_t {
	Master, // 全体（全部の音に掛かる）
	BGM,    // 音楽
	SE,     // 効果音
};

/// <summary>
/// 設定（音の大きさ・全画面）。saves/Settings.json に保存する（進み具合の Progress.json とは別。Reset Data でも消えない）
/// <para>起動したとき（main.cpp）に Load で読んで当てる。設定画面（SettingsMenu）で変えたら、すぐ当てて書く</para>
/// <para>音の大きさの本当の値はエンジン（AudioSystem の全体の音量・種類ごとの音量）が持つ。ここは 0〜10 の段で読み書きするだけ</para>
/// </summary>
class GameSettings {
public:
	static constexpr int kVolumeSteps = 10; // 音の大きさの段の数（0：無音 〜 10：元の大きさ）

	// ファイルを読んで、音の大きさ・全画面を当てる（無い・壊れている → 今のまま）。Engine::Initialize の後に1回
	// 全画面はエディタでは当てない（エディタのウィンドウごと全画面になるので）
	static void Load();

	// ===== 音の大きさ（段。0〜kVolumeSteps）=====
	static int GetVolumeStep(VolumeKind kind);
	// 範囲に収めて当て、ファイルに書く
	static void SetVolumeStep(VolumeKind kind, int step);

	// ===== 全画面 =====
	// 今、全画面か（Alt + Enter で切り替えた後も、今のウィンドウを見る）
	static bool IsFullscreen();
	// 当てて、ファイルに書く
	static void SetFullscreen(bool fullscreen);
};
