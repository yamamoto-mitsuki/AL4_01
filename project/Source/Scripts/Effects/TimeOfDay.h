#pragma once
#include <MyEngine/ECS/Script.h>

#include "Source/Scripts/Effects/DayTimeLook.h"

/// <summary>
/// ステージの時間帯で、見た目をまとめて変える。シーンに1つ、親の無い Entity（名前は TimeOfDay）に付ける
/// <para>ゲームが始まった時（Start）に1回だけ、シーンに置いた値へ時間帯の色を掛ける（置いた値は昼の見た目のまま作ればよい）：</para>
/// <para>平行光源（色・強さ）・空の絵（sky の板の絵を時間帯の絵に替え、色を掛ける）・木の絵（trees の板）・霧（距離・高さ）・
/// ポイントライト / スポットライト・画面に漂う粒（AmbientParticles の Particle System）・足場と壁の明るさ（Polygon Renderer の Intensity）</para>
/// <para>時間帯ごとの値は DayTimeLook.cpp の kPresets（コードの表）。シーンでは Time を選ぶだけ。止めてあるエディタでは変わらない（Play で変わる）</para>
/// <para>時間帯をゆっくり移り変わらせたいとき（タイトル）は、TimeOfDayCycle を使う</para>
/// </summary>
class TimeOfDay : public Script {
public:
	void Start() override;
	void Fields(ComponentUI& ui) override;

private:
	// ===== Inspector に出す・保存する値 =====
	DayTime time_ = DayTime::Noon; // このステージの時間帯
};
