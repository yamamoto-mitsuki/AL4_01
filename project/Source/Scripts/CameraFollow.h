#pragma once
#include <MyEngine/ECS/Script.h>

/// <summary>
/// Target の後ろについて行く（Script の例：LateUpdate・GameObject の項目）
/// <para>Camera Component を付けた Entity に付け、Inspector の Target に Hierarchy から追いかける物を落とす</para>
/// </summary>
class CameraFollow : public Script {
public:
	void LateUpdate() override;
	void Fields(ComponentUI& ui) override;

private:
	GameObject target_;                     // 追いかける物（Hierarchy からドラッグ＆ドロップ。保存は EntityId）
	Vector3 offset_ = {0.0f, 5.0f, -10.0f}; // target_ からどれだけ離れるか
};