#pragma once
#include <vector>

#include <MyEngine/Core/Handle.h>
#include <MyEngine/ECS/Entity.h>

/// <summary>
/// ゲームの中の Text（シーンに置いた操作の説明など。開いている画面の中の物は除く）を、まとめて見えなくする・戻す
/// <para>ぼかし（Depth Of Field）は Text に掛からず、くっきり残ってしまうので、ゲームの絵をぼかしている間は隠す（StageIntro・PauseMenu）</para>
/// <para>Script のメンバーに持って、Capture で覚え、SetAlpha で濃さを変える。Component ではない（保存しない）</para>
/// </summary>
class TextHider {
public:
	// 今ある Text のうち、開いている画面の外の物を、元の透明度と一緒に覚える（画面の Start で呼ぶ。自分の画面はもう開いているので除かれる）
	void Capture();
	// 覚えた Text の透明度を、元の rate 倍にする（0：見えない 〜 1：元のまま）。消えた Text は飛ばす
	void SetAlpha(float rate) const;

private:
	/// <summary>
	/// 覚えた Text 1つ分（元の透明度に戻すため）
	/// </summary>
	struct HiddenText {
		Handle<Entity> entity; // Text の Entity
		float alpha = 1.0f;    // 元の色の a
	};

	std::vector<HiddenText> texts_; // 覚えた Text
};
