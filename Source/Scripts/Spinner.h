#pragma once
#include <MyEngine/ECS/Script.h>

class Spinner : public Script {
public:
	void Update() override;
	void Fields(ComponentUI& ui) override;

private:
	Vector3 speed_ = {0.0f, 90.0f, 0.0f}; // 1秒に回す角度
};