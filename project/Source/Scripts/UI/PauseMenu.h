#pragma once
#include <string>

#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/Vector4.h>

/// <summary>
/// ポーズ画面の項目を選ぶ・決める。ポーズ画面（resources/ui/Pause.uiscreen）の一番上の Entity に付ける
/// <para>項目は、子の Items の子の Text（Hierarchy の上から順）。名前で何をするか決める（Resume：つづける / Restart：やりなおす）</para>
/// <para>ポーズ中（ゲームが止まっている間）も、開いている画面の中の Script なので呼ばれる</para>
/// </summary>
class PauseMenu : public Script {
public:
	void Update() override;
	void Fields(ComponentUI& ui) override;

private:
	// 選んでいる項目を、見た目（色・大きさ）に出す
	void ApplyLook() const;
	// 選んでいる項目を決める（名前で何をするか決める）
	void Decide(const std::string& itemName);

	// ===== Inspector に出す・保存する値 =====
	std::string screenName_ = "Pause";                  // この画面の名前（閉じるときに UIScreen::Close に渡す）
	Vector4 normalColor_ = {1.0f, 1.0f, 1.0f, 1.0f};    // 選んでいない項目の文字の色
	Vector4 selectedColor_ = {1.0f, 0.85f, 0.3f, 1.0f}; // 選んでいる項目の文字の色
	float selectedScale_ = 1.15f;                       // 選んでいる項目の大きさ

	// ===== 実行中だけの値 =====
	int selected_ = 0;       // 選んでいる項目（上から 0, 1, …）
	bool isDecided_ = false; // もう決めた（やりなおすの幕が閉じるまでに、もう一度決めないように）
};