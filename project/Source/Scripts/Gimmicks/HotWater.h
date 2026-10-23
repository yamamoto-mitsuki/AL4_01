#pragma once
#include <MyEngine/ECS/Script.h>

/// <summary>
/// 熱湯。Collider（Box・Is Trigger）を付けた Entity に付ける。見た目は半透明の箱（Primitive Renderer の Surface を Transparent）など
/// <para>氷のキューブが触れたら一発アウト（溶ける）。HP は関係ない</para>
/// </summary>
class HotWater : public Script {
public:
	void Fields(ComponentUI& ui) override;
	void OnTriggerEnter(GameObject other) override; // キューブが触れた
	void OnTriggerStay(GameObject other) override;  // 触れている（シーンの始まりから中にいた時も、ここで溶ける）
};