#pragma once
#include <cstdint>
#include <vector>

#include <MyEngine/ECS/Script.h>

#include "Source/Scripts/Effects/DayTimeLook.h"

/// <summary>
/// 時間帯を、朝 → 昼 → 夕方 → 夜 → 朝…と、ゆっくり移り変わらせる（タイトル画面の背景）。シーンに1つ、親の無い Entity に付ける
/// <para>1つの時間帯を Hold Time 秒そのまま見せ、Blend Time 秒かけて次の時間帯へ混ぜる。全体の速さは Speed（倍率）</para>
/// <para>色（光・空・木・霧・ライト・漂う粒・足場）は、時間帯の値（DayTimeLook の kPresets）を混ぜて、置いた値に掛ける（TimeOfDay と同じ掛け方）</para>
/// <para>空の絵は混ぜられないので、混ぜる前半で空の色を霧の色へ寄せ、真ん中（霧の色になった所）で絵を替えて、後半で次の空の色へ戻す。
/// 半透明の板を重ねて混ぜると、エンジンは半透明をぼかしの後に描くので、混ぜている間だけ空がくっきりしてしまう。空の板は1枚のまま（ぼけたまま）にする</para>
/// <para>ステージには付けない（ステージは TimeOfDay で、時間帯を1つに決める）</para>
/// </summary>
class TimeOfDayCycle : public Script {
public:
	void Start() override;  // 置いた値を覚えて、空の板ごとに時間帯の絵を読む
	void Update() override; // 時間を進めて、今の見た目を書く
	void Fields(ComponentUI& ui) override;

private:
	// 今の時間（elapsed_）の見た目を書く
	void ApplyNow() const;

	// ===== Inspector に出す・保存する値 =====
	DayTime startTime_ = DayTime::Morning; // 始まりの時間帯
	float holdTime_ = 8.0f;                // 1つの時間帯を、そのまま見せる秒数
	float blendTime_ = 6.0f;               // 次の時間帯へ混ぜる秒数（前半で霧の色へ・真ん中で空の絵を替えて・後半で次の空の色へ）
	float speed_ = 1.0f;                   // 時間の速さ（倍率。2 なら倍の速さで移り変わる。0 なら止まる）

	// ===== 実行中だけの値 =====
	float elapsed_ = 0.0f;                           // 始まりの時間帯から進んだ秒数（速さを掛けた後。一回りしたら 0 へ戻す）
	DayTimeLook look_;                               // 置いた値（光・空・木・霧…）
	std::vector<std::vector<uint32_t>> skyTextures_; // 空の板ごとの、時間帯ごとの絵（look_ の空の板と同じ並び。中は DayTime の並び）
};
