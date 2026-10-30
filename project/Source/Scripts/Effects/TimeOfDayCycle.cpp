#include "TimeOfDayCycle.h"

#include <algorithm>
#include <cmath>

#include <externals/magic_enum/magic_enum.hpp>

#include <MyEngine/Components/Rendering2D/SpriteRendererComponent.h>
#include <MyEngine/ECS/EntityManager.h>
#include <MyEngine/Math/Easing.h>
#include <MyEngine/Math/MathUtility.h>

SCRIPT(TimeOfDayCycle, "Effects");

namespace {
constexpr size_t kDayTimeCount = magic_enum::enum_count<DayTime>(); // 時間帯の数
} // namespace

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void TimeOfDayCycle::Fields(ComponentUI& ui) {
	ui.Label("時間帯を 朝 → 昼 → 夕方 → 夜 → 朝… とゆっくり移り変わらせる（タイトルの背景。ステージは TimeOfDay）。止めてあるエディタでは変わらない");
	ui.Label("時間帯ごとの色・空の絵は DayTimeLook.cpp の kPresets（TimeOfDay と同じ）");
	ui.Field("Start Time", startTime_, Tip("始まりの時間帯"));
	ui.Field("Hold Time", holdTime_, AtLeast(0.0f).Tip("1つの時間帯を、そのまま見せる秒数"));
	ui.Field("Blend Time", blendTime_, AtLeast(0.0f).Tip("次の時間帯へ混ぜる秒数。前半で空が霧の色に寄り、真ん中で空の絵が替わり、後半で次の空の色へ戻る"));
	ui.Field("Speed", speed_, AtLeast(0.0f).Tip("時間の速さ（倍率）。2 なら倍の速さで移り変わる・0.5 なら半分の速さ・0 なら止まる"));
}

//=============================================================================
// 最初に1回：置いた値を覚えて、空の板ごとに時間帯の絵を読む
//=============================================================================
void TimeOfDayCycle::Start() {
	look_.Capture();
	skyTextures_.clear();
	for (const DayTimeLook::SkySprite& sky : look_.GetSkySprites()) {
		std::vector<uint32_t>& textures = skyTextures_.emplace_back();
		for (const DayTime time : magic_enum::enum_values<DayTime>()) {
			textures.push_back(DayTimeLook::LoadSkyTexture(time, sky.texture)); // 夕方は置いた絵のまま
		}
	}
	ApplyNow();
}

//=============================================================================
// 毎フレーム：時間を進めて、今の見た目を書く
//=============================================================================
void TimeOfDayCycle::Update() {
	// 一回りの秒数で戻す（ずっと増え続けて、小数の細かさが落ちないように）
	const float cycleTime = (holdTime_ + blendTime_) * static_cast<float>(kDayTimeCount);
	elapsed_ += Time::GetUnscaleDeltaTime() * speed_;
	if (cycleTime > 0.0f) {
		elapsed_ = std::fmod(elapsed_, cycleTime);
	}
	ApplyNow();
}

void TimeOfDayCycle::ApplyNow() const {
	// --- 今どの時間帯か・次へどれだけ混ざったか ---
	// 1つの時間帯は Hold Time 秒そのまま → Blend Time 秒で次へ。混ぜ方は、始めと終わりをゆっくりに（InOutSine）
	const float phaseTime = (std::max)(holdTime_ + blendTime_, 0.001f);
	const float phases = elapsed_ / phaseTime;
	const size_t start = magic_enum::enum_index(startTime_).value_or(0);
	const size_t current = (start + static_cast<size_t>(phases)) % kDayTimeCount;
	const size_t next = (current + 1) % kDayTimeCount;
	const float intoPhase = (phases - std::floor(phases)) * phaseTime; // この時間帯になってからの秒数
	const float linear = (blendTime_ > 0.0f) ? std::clamp((intoPhase - holdTime_) / blendTime_, 0.0f, 1.0f) : 0.0f;
	const float blend = Ease::InOutSine(linear, 0.0f, 1.0f);

	// --- 色：2つの時間帯の値を混ぜて、置いた値に掛ける ---
	const DayTimePreset preset = LerpDayTimePreset(GetDayTimePreset(magic_enum::enum_value<DayTime>(current)), GetDayTimePreset(magic_enum::enum_value<DayTime>(next)), blend);
	look_.Apply(preset);

	// --- 空の絵：混ぜる前半で霧の色へ寄せ、真ん中で絵を替えて、後半で次の空の色へ戻す ---
	// haze：0（空の色のまま）〜 1（霧の色。混ぜる真ん中）。真ん中を過ぎたら、次の時間帯の絵にする
	const float haze = 1.0f - std::abs(linear * 2.0f - 1.0f);
	const size_t shown = (linear < 0.5f) ? current : next;
	const Vector3 hazeColor = look_.GetHazeColor(preset);
	const std::vector<DayTimeLook::SkySprite>& skies = look_.GetSkySprites();
	for (size_t i = 0; i < skies.size() && i < skyTextures_.size(); ++i) {
		SpriteRendererComponent* sprite = EntityManager::Get<SpriteRendererComponent>(skies[i].entity);
		if (sprite == nullptr) {
			continue;
		}
		sprite->textureHandle = skyTextures_[i][shown];
		const Vector3 skyColor = {sprite->color.x, sprite->color.y, sprite->color.z}; // Apply で空の色を掛けた後
		const Vector3 color = MathUtility::Lerp(skyColor, hazeColor, Ease::InOutSine(haze, 0.0f, 1.0f));
		sprite->color = {color.x, color.y, color.z, sprite->color.w};
	}
}
