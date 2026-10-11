#pragma once
#include <MyEngine/ECS/Script.h>

/// <summary>
/// 回りながら待っていて、PlayerController に拾われる（Script の例：Update・OnDestroy）
/// <para>Collider を一緒に付ける</para>
/// </summary>
class Coin : public Script {
public:
	void Update() override;
	void OnDestroy() override;
	void Fields(ComponentUI& ui) override;

private:
	float spinSpeed_ = 180.0f; // 1秒に回す角度（度）
};