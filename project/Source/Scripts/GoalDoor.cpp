#include "GoalDoor.h"

#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/Systems/Effects/ParticleSystem.h>

#include "IceCube.h"
#include "LevelInfo.h"
#include "Source/Scripts/Audio/GameAudio.h"
#include "Source/Scripts/Effects/GameRumble.h"

SCRIPT(GoalDoor, "Game");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void GoalDoor::Fields(ComponentUI& ui) {
	ui.Label("キューブが触れたら：粒が昇る・ゲームの絵がぼけていく → Screen Delay 秒後にクリア画面（シーンは替えない）");
	// --- クリア画面 ---
	ui.Field("Clear Screen", clearScreen_, Tip("出す画面の名前（resources/ui の .uiscreen の名前）。無ければ、次のレベルへ進む"));
	ui.Field("Screen Delay", screenDelay_, AtLeast(0.0f).Tip("キューブが触れてから、クリア画面を出すまでの秒数（キューブが扉へ吸い込まれる間）"));
	// --- 昇る粒 ---
	ui.Field("Particle Name", particleName_, Tip("触れたときに流し始める Particle System の名前（この Entity の直下の子）。Play On Start を外して置く"));
	// --- ぼかし ---
	ui.Field("Blur Time", blurTime_, AtLeast(0.01f).Tip("触れてから、ゲームの絵がぼけきるまでの秒数（ゆっくり始まって、ゆっくり止まる）"));
	ui.Field("Blur Focus Distance", blurTo_.focusDistance, AtLeast(0.0f).Tip("ぼけきった時のピントの奥行き（m）。小さいほど、遊ぶ面まで全部ぼける"));
	ui.Field("Blur Focus Range", blurTo_.focusRange, AtLeast(0.0f).Tip("ぼけきった時の、ぼけない幅（m）。0 で、ピントの所もすぐぼけ始める"));
	ui.Field("Blur Far Transition", blurTo_.farTransition, AtLeast(0.0f).Tip("ぼけきった時の、一番ぼけるまでの奥行き（m）。小さいほど、すぐ奥が強くぼける"));
	ui.Field("Blur Near Transition", blurTo_.nearTransition, AtLeast(0.0f).Tip("ぼけきった時の、手前のぼけ（m）。0 なら手前はぼかさない"));
	ui.Field("Blur Max", blurTo_.maxBlur, AtLeast(0.0f).Tip("ぼけきった時の、ぼけの半径（ピクセル。エンジンの上限は 32）。大きいほど強くぼける"));
	// --- 振動 ---
	ui.Field("Rumble Power", rumblePower_, Range(0.0f, 1.0f).Tip("触れた瞬間のコントローラーの振動の強さ（一定）"));
	ui.Field("Rumble Time", rumbleTime_, AtLeast(0.0f).Tip("震える秒数。0 なら震えない"));
}

//=============================================================================
// 当たり
//=============================================================================
void GoalDoor::OnTriggerEnter(GameObject other) {
	// --- 早期リターン：キューブでない・もうやられた・もうクリアした ---
	IceCube* cube = other.GetComponent<IceCube>();
	if (cube == nullptr || !cube->IsPlayable() || isCleared_) {
		return;
	}
	// --- クリア：キューブを扉へ吸い込ませて、時間を数え始める ---
	cube->Clear(GetWorldPosition(transform));
	isCleared_ = true;
	// --- レベルにクリアを知らせる（時間を止めて、進み具合に記録する）---
	if (LevelInfo* level = LevelInfo::Find()) {
		level->Clear();
	}
	// --- 音：ゴールの音を鳴らして、BGM はぼけるのと同じ秒数で小さくする（次のレベル・タイトルが始まったら戻る）---
	GameAudio::PlaySe(Se::Goal);
	GameAudio::DuckBgm(blurTime_);
	// --- コントローラーを震わせる（止めるのは GameRumble。数えるのは IceCube::Update で、クリアの後も動いている）---
	GameRumble::Play(rumblePower_, rumbleTime_);
	// --- 粒が昇り始める ---
	if (const GameObject particle = gameObject.FindChild(particleName_)) {
		ParticleSystem::Play(particle.handle);
	} else {
		LogManager::Warning("GoalDoor：子に「" + particleName_ + "」が無いので、粒は昇りません（子の名前と Particle Name を同じにする）");
	}
	// --- ゲームの絵をぼかし始める ---
	if (!blur_.Begin(blurTo_, blurTime_)) {
		LogManager::Warning("GoalDoor：シーンに Depth Of Field が無いので、ぼかしません（Main Camera に付けると、クリアの時にぼける）");
	}
}

//=============================================================================
// 毎フレーム
//=============================================================================
void GoalDoor::Update() {
	if (!isCleared_) {
		return;
	}
	const float deltaTime = Time::GetDeltaTime();
	blur_.Update(deltaTime);
	if (isScreenRequested_) {
		return;
	}
	timer_ += deltaTime;
	if (timer_ < screenDelay_) {
		return;
	}
	// --- クリア画面を出す（画面が無ければ、次のレベルへ。LevelInfo も無ければ、このレベルを最初から）---
	isScreenRequested_ = true;
	if (UIScreen::Open(clearScreen_)) {
		return;
	}
	LogManager::Warning("GoalDoor：クリア画面を出せなかったので、次のレベルへ進みます（resources/ui/" + clearScreen_ + ".uiscreen が無い？）");
	if (const LevelInfo* level = LevelInfo::Find()) {
		level->LoadNextLevel();
	} else {
		SceneManager::ReloadScene();
	}
}