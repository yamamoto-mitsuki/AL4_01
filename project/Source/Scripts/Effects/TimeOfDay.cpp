#include "TimeOfDay.h"

#include <MyEngine/Components/Rendering2D/SpriteRendererComponent.h>
#include <MyEngine/ECS/EntityManager.h>

SCRIPT(TimeOfDay, "Effects");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void TimeOfDay::Fields(ComponentUI& ui) {
	ui.Label("ゲームが始まった時に、光・空の絵・木の絵・霧・ライト・漂う粒の色・足場と壁の明るさを、時間帯の色にする（止めてあるエディタでは変わらない）");
	ui.Label("時間帯ごとの値は DayTimeLook.cpp の kPresets。シーンは昼の少し前の見た目で作っておく");
	ui.Field("Time", time_, Tip("このステージの時間帯（Morning：朝・Noon：昼・Evening：夕方・Night：夜）"));
}

//=============================================================================
// 最初に1回：時間帯の色を掛ける
//=============================================================================
void TimeOfDay::Start() {
	// 置いた値を覚えて、時間帯の色を掛ける（1回だけなので、覚えた物は持っておかない）
	DayTimeLook look;
	look.Capture();
	look.Apply(GetDayTimePreset(time_));
	// --- 空の板の絵を、時間帯の絵に替える（絵が同じ大きさなので、板の大きさ＝Pixels Per Unit は変わらない）---
	for (const DayTimeLook::SkySprite& sky : look.GetSkySprites()) {
		if (SpriteRendererComponent* sprite = EntityManager::Get<SpriteRendererComponent>(sky.entity)) {
			sprite->textureHandle = DayTimeLook::LoadSkyTexture(time_, sky.texture);
		}
	}
}
