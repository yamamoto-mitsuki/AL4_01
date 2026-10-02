#include "Player.h"

#include <span>
#include <vector>
#include <algorithm>

#include <MyEngine/ECS/GameComponent.h>
#include <MyEngine/Camera/Camera.h>
#include <MyEngine/Collision/Collision.h>
#include <MyEngine/Components/TrailComponent.h>
#include <MyEngine/Input/InputManager.h>
#include <MyEngine/Time/Time.h>

#include "InkEnemy.h"
#include "InkWall.h"

// データは Player.h


// =============================================================================
// Inspector での見せ方
// =============================================================================
COMPONENT(Player, "InkSlash") {
	ui.Separator("Movement");
	ui.Field("Move Speed", value.moveSpeed, AtLeast(0.0f).Tip("1秒に進む距離"));
	ui.Field("Dash Speed", value.dashSpeed, AtLeast(0.0f).Tip("線に沿ってダッシュする1秒に進む距離"));
	ui.Field("Slow Scale", value.slowScale, Range(0.01f, 1.0f).Tip("描いている間の時間の速さ"));
	ui.Field("Point Spacing", value.pointSpacing, AtLeast(0.01f).Tip("マウスがこの距離動くたびに点を足す"));
	ui.Field("Slash Radius", value.slashRadius, AtLeast(0.0f).Tip("斬る太さ（動いた跡のカプセルの半径）"));
	ui.Field("Pickup Radius", value.pickupRadius, AtLeast(0.0f).Tip("インクを回収する距離"));
	ui.Field("Hit Stop Time", value.hitStopTime, AtLeast(0.0f).Tip("敵を倒したときに止める秒数"));
	ui.Field("Wall Margin", value.wallMargin, AtLeast(0.0f).Tip("壁の手前で止める距離（壁にめり込まないように）"));
	
	ui.Separator("Ink Line");
	ui.Field("Max Ink", value.maxInk, AtLeast(0.0f).Tip("描ける線の長さの上限"));
	ui.Field("Draw Line Width", value.drawLineWidth, AtLeast(0.0f).Tip("書いた線の太さ（TrailComponent の width に掛ける）"));
	ui.ColorField("Draw Line Color", value.drawLineColor);
	ui.Field("Ink Height", value.inkHeight, AtLeast(0.0f).Tip("インクの高さ（地面のインクの厚み）"));
}

// =============================================================================
// 処理
// =============================================================================
namespace {
// --- 相手の見分け ---
bool IsWall(Handle<Entity> entity) { return EntityManager::Get<InkWall>(entity) != nullptr; }
bool IsEnemy(Handle<Entity> entity) { return EntityManager::Get<InkEnemy>(entity) != nullptr; }

// --- マウスが指している地面（y = 0）の位置。ゲーム画面の外・地面を指していないなら false ---
bool GetMouseGround(const Player& player, Vector3& ground) {
	const Camera* camera = CameraSystem::GetMainCamera();
	if (camera == nullptr || !InputManager::IsMouseInGame()) {
		return false;
	}

	const Ray ray = camera->ScreenPointToRay(InputManager::GetMousePositionInGame());
	float t = 0.0f;
	if (!Collision::RayPlane(
	        ray,
	        {
	            {0.0f, 1.0f, 0.0f},
                0.0f
    },
	        &t)) {
		return false;
	}
	ground = Collision::RayPoint(ray, t);
	ground.y = player.inkHeight; // インクの高さにする
	return true;
}

// --- 書いた線。新しい線を作る（地面に寝かせた Trail。点は AddPoint で足す） ---
Handle<Entity> CreateLine(const Player& player, const Vector3& start) {
	TrailComponent trail;
	trail.emitting = false;
	trail.lifeTime = 0.0f; // 消えない
	trail.maxPoints = 1024;
	trail.width = player.drawLineWidth;
	trail.endWidthRate = 1.0f; // 太さを一定
	trail.alignment = TrailAlignment::Ground;
	trail.color = {player.drawLineColor.x, player.drawLineColor.y, player.drawLineColor.z, 1.0f};
	trail.blend = TrailBlend::Normal;

	const Handle<Entity> line = EntityManager::Create("InkLine");
	EntityManager::RequestAdd<TrailComponent>(line, trail);
	TrailSystem::AddPoint(line, start);
	return line;
}

// --- 線から radius 以内にいるか（線分ごとに一番近い点を見る） ---
bool IsTouchingLine(Handle<Entity> line, const Vector3& position, float radius) {
	const std::span<const TrailPoint> points = TrailSystem::GetPoints(line);
	for (size_t i = 1; i < points.size(); ++i) {
		const Vector3 closest = Collision::ClosestPointOnSegment(position, points[i - 1].position, points[i].position);
		if (LengthSq(closest - position) <= radius * radius) {
			return true;
		}
	}
	return false;
}

// ---　歩く・インクを拾う・書き始める ---
void UpdateMove(Player& player, TransformComponent& transform, float deltaTime) {
	// 移動
	const Vector2 move = InputManager::GetMoveAxis(false);
	transform.translation += Vector3{move.x, 0.0f, move.y} * player.moveSpeed * deltaTime;

	const Vector3 feet = {transform.translation.x, player.inkHeight, transform.translation.z};
	const Handle<Entity> lastLine = EntityManager::Find(player.line);
	// 地面のインクに触れたら回収する（動いた後と最後に引いた線は拾わない）
	if (InputManager::IsMouseButtonPressed(1)) {
		EntityManager::ForEach<TrailComponent>([&player, &feet, lastLine](Handle<Entity> line, const TrailComponent& trail) {
			if (trail.emitting || line == lastLine || !IsTouchingLine(line, feet, player.pickupRadius)) {
				return;
			}
			player.ink = (std::min)(player.maxInk, player.ink + TrailSystem::CalcLength(line));
			EntityManager::Destroy(line);
		});

		return; // 回収したら書き始めない
	}
	// 左クリックで書き始める
	if (InputManager::IsMouseButtonTriggered(0) && player.ink > 0.0f) {
		Time::SetGameTimeScale(player.slowScale); // 書いている間は時間を遅くする
		player.line = EntityManager::RefOf(CreateLine(player, feet));
		player.isBlocked = false;
		player.state = PlayerState::Draw;
	}
}

// --- 線を伸ばす（壁とインクの残りで止める） ---
void ExtendLine(Player& player, Handle<Entity> line) {
	Vector3 target;
	const std::span<const TrailPoint> points = TrailSystem::GetPoints(line);
	// マウスが地面を指していない・壁に当たった・インクが無くなったら止める
	if (player.isBlocked || points.empty() || !GetMouseGround(player, target)) {
		return;
	}

	const Vector3 last = points.back().position; // AddPoint すると points は使えなくなるので、先に写す
	const Vector3 toTarget = target - last;
	float distance = Length(toTarget);
	if (distance < player.pointSpacing) {
		return; // 短すぎて点を足せない
	}
	const Vector3 direction = toTarget / distance;

	// 壁: 最後の点からマウスの方へレイを飛ばし、当たったら壁の手前まで
	RaycastHit hit;
	if (CollisionSystem::Raycast({last, direction}, distance, hit, IsWall)) {
		distance = (std::max)(hit.distance - player.wallMargin, 0.0f);
		player.isBlocked = true;
	}
	// インク：残りより長ければ、残りの分まで
	if (distance >= player.ink) {
		distance = player.ink;
		player.isBlocked = true;
	}
	if (distance <= 0.0f) {
		return;
	}
	TrailSystem::AddPoint(line, last + direction * distance);
	player.ink -= distance;
}

// --- 線に沿って進み、触れた敵を倒す ---
void UpdateDash(Player& player, TransformComponent& transform, float deltaTime) { 
	const Handle<Entity> line = EntityManager::Find(player.line);
	const std::span<const TrailPoint> points = TrailSystem::GetPoints(line);
	const int pointCount = static_cast<int>(points.size());

	// このフレームに進める距離を、点から点へ使っていく（速いと1フレームで点をいくつも超える）
	const Vector3 start = transform.translation;
	Vector3 position = start;
	float remaining = player.dashSpeed * deltaTime;
	while (remaining > 0.0f && player.dashIndex < pointCount) {
		Vector3 target = points[player.dashIndex].position;
		target.y = position.y;
		const Vector3 toTarget = target - position;
		const float distance = Length(toTarget);
		if (distance <= remaining) {
			position = target;
			remaining -= distance;
			++player.dashIndex;
		} else {
			position += toTarget * (remaining / distance);
			remaining = 0.0f;
		}
	}
	transform.translation = position;

	// 斬る：このフレームに動いた跡（start → position）に触れた敵。速くても飛び越えない
	std::vector<Handle<Entity>> touched;
	CollisionSystem::SweepSphere({start, position, player.slashRadius}, touched, IsEnemy);
	for (const Handle<Entity> enemyEntity : touched) {
		InkEnemy* enemy = EntityManager::Get<InkEnemy>(enemyEntity);
		if (enemy == nullptr || enemy->hp <= 0) {
			continue;
		}
		--enemy->hp;
		if (enemy->hp <= 0) {
			EntityManager::Destroy(enemyEntity);
			Time::HitStop(player.hitStopTime);
		}
	}

	// 線の終わりまで来たら歩きに戻る（線はインクとして地面に残る）
	if (player.dashIndex >= pointCount) {
		player.state = PlayerState::Move;
	}
}

} // namespace


void PlayerUpdate(Player& player, TransformComponent& transform, float deltaTime) { 
	switch (player.state) {
	case PlayerState::Idle: // 停止
	case PlayerState::Move: // 移動
		UpdateMove(player, transform, deltaTime);
		break;
	// --- 線を書いているとき ---
	case PlayerState::Draw:
		// 書いている間は動けない
		ExtendLine(player, EntityManager::Find(player.line));
		if (!InputManager::IsMouseButtonPressed(0)) {
			// 離した：時間を戻して、線の始めから進む
			Time::SetGameTimeScale(1.0f);
			player.dashIndex = 0;
			player.state = PlayerState::Dash;
		}
		break;
	// --- ダッシュしているとき ---
    case PlayerState::Dash:
		UpdateDash(player, transform, deltaTime);
		break;
	}
}
SYSTEM(PlayerUpdate);