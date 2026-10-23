#pragma once
#include <MyEngine/ECS/Script.h>


/// <summary>
/// WASD で歩き、触れたコインを拾う（Script の例：Fields・Start・Update・OnTriggerEnter・Destroy）
/// <para>Collider を一緒に付ける（Add Component → Physics → Collider）</para>
/// </summary>
class PlayerController : public Script {
public:
	void Start() override;
	void Update() override;
	void OnTriggerEnter(GameObject other) override;
	void Fields(ComponentUI& ui) override;

	// 拾ったコインの数（ほかの Script から読む用）
	int GetCoinCount() const { return coinCount_; }

private:
	float moveSpeed_ = 5.0f; // 1秒に進む距離（Fields に書いたので、Inspector に出て保存される）
	int coinCount_ = 0;      // 拾ったコインの数（Fields に書いていないので、実行中だけの値）
};