#pragma once
#include <MyEngine/ECS/Script.h>

/// <summary>
/// 緑の壁（くっつく壁）の印。Collider の付いた足場（Polygon Shape など。Is Trigger は外す）に付ける
/// <para>空中のキューブが横から当たるとくっつく（止まって、自分のジャンプが戻る）。上に乗ったら普通の床</para>
/// <para>中身は無い。くっつく動きは全部 IceCube で、IceCube が「当たった相手に StickyWall が付いているか」を見る</para>
/// </summary>
class StickyWall : public Script {
public:
	void Fields(ComponentUI& ui) override;
};