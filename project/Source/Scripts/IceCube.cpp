#include "IceCube.h"

#include <algorithm>
#include <cmath>

#include <MyEngine/Input/InputManager.h>
#include <MyEngine/Math/Easing.h>
#include <MyEngine/Math/MathUtility.h>
#include <MyEngine/Math/RandomEngine.h>

#include "LevelInfo.h"
#include "Source/Scripts/Gimmicks/JumpOrb.h"
#include "Source/Scripts/Gimmicks/LaunchOrb.h"
#include "Source/Scripts/Gimmicks/StickyWall.h"

SCRIPT(IceCube, "Game");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void IceCube::Fields(ComponentUI& ui) {
	ui.Label("A・D / ←→ / 左スティック・十字：移動　Space・W・↑ / A ボタン：ジャンプ（長押しで高く）");
	// ===== 移動 =====
	ui.Separator("Run");
	ui.Field("Run Speed", runSpeed_, AtLeast(0.0f).Tip("左右の一番速い速さ（m/s）"));
	ui.Field("Ground Accel", groundAccel_, AtLeast(0.0f).Tip("地上で、入力の向きへ速さを変える強さ（1秒に何 m/s 変えるか）"));
	ui.Field("Ground Decel", groundDecel_, AtLeast(0.0f).Tip("地上で入力が無いとき、止まる強さ"));
	ui.Field("Air Accel", airAccel_, AtLeast(0.0f).Tip("空中で、入力の向きへ速さを変える強さ"));
	ui.Field("Air Decel", airDecel_, AtLeast(0.0f).Tip("空中で入力が無いとき、横の速さを落とす強さ（小さいと、跳んだ勢いが残る）"));
	// ===== ジャンプ =====
	ui.Separator("Jump");
	ui.Field("Jump Height", jumpHeight_, AtLeast(0.0f).Tip("長押ししたときの高さ（m）。跳ぶ速さは、この高さと Gravity から計算する"));
	ui.Field("Gravity", gravity_, AtLeast(0.1f).Tip("重力の強さ（m/s²。上がっている間）。大きいほどキビキビ動く（Rigidbody の Gravity Scale は、これに合わせて毎フレーム書く）"));
	ui.Field("Fall Gravity Multiplier", fallGravityMultiplier_, AtLeast(0.0f).Tip("落ちる間の重力の倍率（1 より大きいと、ふわっとしない）"));
	ui.Field("Max Fall Speed", maxFallSpeed_, AtLeast(0.0f).Tip("落ちる速さの上限（m/s）"));
	ui.Field("Jump Cut", jumpCutRatio_, Range(0.0f, 1.0f).Tip("上がっている途中でジャンプを離したとき、上向きの速さに掛ける（0.5 なら、すぐ離すと低いジャンプ。1 なら長押しの差が無い）"));
	ui.Field("Coyote Time", coyoteTime_, AtLeast(0.0f).Tip("足場から落ちても、この秒数はまだジャンプできる（端ぎりぎりで押しても跳べるように）"));
	ui.Field("Jump Buffer", jumpBufferTime_, AtLeast(0.0f).Tip("着地のこの秒数前に押したジャンプは、着地した瞬間に跳ぶ（先行入力）"));
	// ===== ギミック =====
	ui.Separator("Gimmick");
	ui.Field("Wall Jump Speed", wallJumpSpeed_, AtLeast(0.0f).Tip("緑の壁から跳んだときの、壁と反対への横の速さ（m/s）。上へは Jump Height と同じ高さ"));
	ui.Field("Wall Regrab Time", wallRegrabTime_, AtLeast(0.0f).Tip("緑の壁から離れた（跳んだ・離れる向きに入力した）後、この秒数は同じ壁にくっつかない（押しっぱなしで、すぐ戻ってくっつかないように）"));
	ui.Field("Launch Gather Time", launchGatherTime_, AtLeast(0.0f).Tip("緑の円の中でジャンプしてから、円の真ん中に寄る秒数（その後、矢印の向きへ飛ぶ。0 なら寄らずにすぐ飛ぶ）"));
	ui.Field("Launch Jump Lock", launchJumpLock_, AtLeast(0.0f).Tip("吹っ飛び始めてから、この秒数はジャンプできない（その後のジャンプで、吹っ飛びをやめて跳ぶ。別の緑の円の中なら、いつでも吹っ飛び直せる）"));
	ui.Field("Launch Jump Keep", launchJumpKeep_, Range(0.0f, 1.0f).Tip("吹っ飛びをやめて跳んだとき、横の速さに掛ける（0.5 なら半分残る）"));
	ui.Field("Launch Spin Speed", launchSpinSpeed_, AtLeast(0.0f).Tip("吹っ飛び中に、見た目のキューブが回る速さ（度/秒。飛ぶ向きへ転がるように回る）"));
	// ===== HP・やられ・クリア =====
	ui.Separator("Life");
	ui.Field("Max HP", maxHp_, Range(1.0f, 10.0f).Tip("HP（トゲで減る。0 で砕ける）"));
	ui.Field("Restart Delay", restartDelay_, AtLeast(0.0f).Tip("やられてから、レベルの最初に戻るまでの秒数"));
	ui.Field("Clear Time", clearTime_, AtLeast(0.01f).Tip("ゴールの扉へ吸い込まれる秒数"));
	// ===== ダメージ・やられの見た目 =====
	ui.Separator("Damage");
	ui.AssetField("Crack Small Texture", crackSmallTexture_, AssetType::Texture, Tip("1 回ダメージを受けたときの、子の Body のテクスチャ（小さいひび）。無ければ元のまま"));
	ui.AssetField("Crack Large Texture", crackLargeTexture_, AssetType::Texture, Tip("2 回以上ダメージを受けたときの、子の Body のテクスチャ（大きいひび）。無ければ小さいひびのまま"));
	ui.Field("Hurt Flash Time", hurtFlashTime_, AtLeast(0.0f).Tip("ダメージを受けたとき、白く光る秒数"));
	ui.Field("Hurt Shake Angle", hurtShakeAngle_, AtLeast(0.0f).Tip("ダメージを受けたとき、揺れる角度（度）"));
	ui.Field("Hurt Shake Time", hurtShakeTime_, AtLeast(0.0f).Tip("ダメージを受けたとき、揺れる秒数"));
	ui.Field("Shard Count", shardCount_, Range(0.0f, 32.0f).Tip("砕けたときのかけらの数（かけらは Body と同じ見た目の小さな箱）"));
	ui.Field("Shard Size", shardSize_, AtLeast(0.0f).Tip("かけらの大きさ（Cube Size に掛ける）"));
	ui.Field("Shard Speed", shardSpeed_, AtLeast(0.0f).Tip("かけらが飛び散る速さ（m/s。一番速いかけら。遅いかけらはこの半分）"));
	ui.Field("Melt Time", meltTime_, AtLeast(0.01f).Tip("熱湯で溶けて、つぶれきるまでの秒数"));
	ui.Field("Melt Sink Speed", meltSinkSpeed_, AtLeast(0.0f).Tip("溶けながら沈む速さ（m/s）"));
	// ===== 風 =====
	ui.Separator("Wind");
	ui.Field("Wind Wobble Angle", windWobbleAngle_, AtLeast(0.0f).Tip("風の中で、見た目のキューブがぐらぐら揺れる角度（度）。流される速さは Wind Zone の Speed・Strength で決まる"));
	// ===== 見た目（子の Body）=====
	ui.Separator("Look");
	ui.Field("Cube Size", cubeSize_, AtLeast(0.0f).Tip("見た目のキューブの大きさ（m）。子の「Body」の Scale に毎フレーム書く。当たりの球の大きさは Collider で決める"));
	ui.Field("Tilt Angle", tiltAngle_, AtLeast(0.0f).Tip("空中で、進む向きに傾く角度（度。走る速さのとき）"));
	ui.Field("Tilt Speed", tiltSpeed_, AtLeast(0.0f).Tip("傾きが変わる速さ（大きいほどすぐ変わる）"));
	ui.Field("Jump Stretch", jumpStretch_, Range(0.0f, 0.9f).Tip("跳んだとき、縦に伸びる割合（0 なら伸びない）"));
	ui.Field("Land Squash", landSquash_, Range(0.0f, 0.9f).Tip("着地したとき、縦につぶれる割合（落ちる速さの上限で着地したとき。ゆっくり着地すると小さい）"));
	ui.Field("Stretch Time", stretchTime_, AtLeast(0.01f).Tip("伸び縮みが戻る時間の目安（秒）"));
}

//=============================================================================
// 最初に1回 / 毎フレーム
//=============================================================================
// ===== 最初に1回 =====
void IceCube::Start() {
	hp_ = maxHp_;
	body_ = gameObject.FindChild("Body"); // 見た目のキューブ（無ければ、見た目は動かさない）
	// Body の元の色・テクスチャ（ひび・白く光るのを、ここから変えて戻す）
	if (body_) {
		if (const PrimitiveRendererComponent* renderer = body_.GetComponent<PrimitiveRendererComponent>()) {
			bodyColor_ = renderer->material.color;
			bodyTexture_ = renderer->material.textureHandle;
		}
	}
	// 風は自分で読んで動く（UpdateWindDrift・ApplyWindLift）ので、エンジンには押させない
	// エンジンに押させると、左右の入力（UpdateRun）が毎フレーム速さを目標へ戻して、横の風が消されてしまう
	if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) {
		rigidbody->windScale = 0.0f;
	}
	// 下の線の高さは、シーンの LevelInfo から（LevelInfo が無ければ -10 のまま）
	if (const LevelInfo* level = LevelInfo::Find()) {
		killHeight_ = level->GetKillHeight();
	}
}

// ===== 毎フレーム =====
void IceCube::Update() {
	RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>();
	if (rigidbody == nullptr) {
		return; // Rigidbody が無いと動かせない（Inspector で外した）
	}
	const float deltaTime = Time::GetDeltaTime();
	if (deltaTime <= 0.0f) {
		return; // ヒットストップ中（入力で速さを変えない）
	}
	// --- 下の線より下に落ちたら、一発アウト（動ける間はいつでも。くっつき・吹っ飛びの最中も）---
	if (IsPlayable() && GetWorldPosition(transform).y < killHeight_) {
		Kill(DeathType::Fall);
	}
	// --- トゲなどで頼まれたダメージを、1回分だけ受ける（同じフレームに2つのトゲに触れても1回）---
	ApplyDamage(*rigidbody);
	// --- 自分の所の風（前のフレームの終わりの Wind Zone から）---
	wind_ = WindZoneSystem::Sample(GetWorldPosition(transform));
	wallRegrabTimer_ -= deltaTime;

	// --- 今していることで、することが変わる ---
	switch (state_) {
	case State::Move:
		UpdateMove(*rigidbody, deltaTime);
		break;
	case State::Stick:
		UpdateStick(*rigidbody, deltaTime);
		break;
	case State::Launch:
		UpdateLaunch(*rigidbody, deltaTime);
		break;
	case State::Dead:
		UpdateDead(deltaTime);
		break;
	case State::Clear:
		UpdateClear(deltaTime);
		break;
	}
	UpdateBody(*rigidbody, deltaTime);

	// --- 次のフレームのために覚える・空にする ---
	previousVelocityX_ = rigidbody->velocity.x;
	previousVelocityY_ = rigidbody->velocity.y;
	// 当たりで集める物は、次のフレームの当たりで集め直す
	groundNormal_ = {0.0f, 0.0f, 0.0f};
	wallContact_ = {};
	touchingOrbs_.clear();
	isLaunchBlocked_ = false;
}

//=============================================================================
// その状態のときの更新
//=============================================================================
// ===== 地上 =====
void IceCube::UpdateMove(RigidbodyComponent& rigidbody, float deltaTime) {
	UpdateGround(rigidbody, deltaTime);
	// --- 空中で、横から緑の壁に触れたら、くっつく ---
	if (TryStick(rigidbody)) {
		return;
	}
	UpdateWindDrift(deltaTime);
	UpdateRun(rigidbody, deltaTime);
	UpdateJump(rigidbody, deltaTime);
	if (state_ != State::Move) {
		return; // 緑の円で吹っ飛び始めた（重力は Launch が切った。ここで戻さない）
	}
	UpdateGravity(rigidbody);
	ApplyWindLift(rigidbody, deltaTime); // 重力の後（上向きの風が、重力より強ければ浮く）
}

// ===== 緑の壁にくっついている（止まる。ジャンプで壁と反対へ跳ぶ・離れる向きの入力で落ちる）=====
void IceCube::UpdateStick(RigidbodyComponent& rigidbody, float deltaTime) {
	// --- 壁ジャンプ：上＋壁と反対へ（先行入力も使う。くっつく少し前に押していたら、くっついた瞬間に跳ぶ）---
	if (ReadJumpRequest(deltaTime)) {
		// 緑の円の中なら、壁ジャンプではなく吹っ飛ぶ（Move のときと同じく、緑の円が先）
		if (UseLaunchOrb()) {
			return;
		}
		ChangeState(State::Move);
		hasAirJump_ = false; // 壁で戻った自分のジャンプを使った
		rigidbody.useGravity = true;
		Jump(rigidbody);
		// 横は、壁の外向き（normal の X の符号）へ Wall Jump Speed。斜めの壁でも、横の速さは同じにする
		// std::copysign(a, b)：a の大きさに、b の符号（＋−）を付けた数（<cmath>）
		rigidbody.velocity.x = std::copysign(wallJumpSpeed_, stickNormal_.x);
		return;
	}
	// --- 壁から離れる向きに入力したら、離れて落ちる（自分のジャンプは残る）---
	if (ReadMoveInput() * stickNormal_.x > kLetGoInput) {
		ChangeState(State::Move);
		rigidbody.velocity = {0.0f, 0.0f, 0.0f};
		rigidbody.useGravity = true;
		return;
	}
	// --- くっついている間：止まって、壁へ少しだけ押し付ける（物理が押し戻して消すので動かない。壁の当たりが続く）---
	rigidbody.useGravity = false;
	rigidbody.velocity = -stickNormal_ * kWallStickSpeed;
}

// ===== やられ（縮んで消えて、少し待ってから、レベルの最初から）=====
void IceCube::UpdateDead(float deltaTime) {
	stateTimer_ += deltaTime;
	// --- 見た目：やられ方で変わる ---
	switch (deathType_) {
	case DeathType::Shatter:
		UpdateShards(deltaTime); // かけらが飛び散る（元のキューブは Kill で隠した）
		break;
	case DeathType::Melt:
		// つぶれながら横に広がって（UpdateBody が meltAmount_ を使う）、ゆっくり沈む
		meltAmount_ = std::clamp(stateTimer_ / meltTime_, 0.0f, 1.0f);
		EntityManager::TranslateWorld(gameObject.handle, {0.0f, -meltSinkSpeed_ * deltaTime, 0.0f});
		break;
	case DeathType::Fall:
		break; // 画面の外なので、何もしない
	}
	// --- 少し待ってから、レベルの最初から（1回だけ頼む）---
	if (stateTimer_ >= restartDelay_ && !isRestartRequested_) {
		isRestartRequested_ = true;
		SceneManager::ReloadScene();
	}
}

// ===== クリア（扉へ吸い込まれる）=====
void IceCube::UpdateClear(float deltaTime) {
	stateTimer_ += deltaTime;
	// --- 扉へ吸い込まれる：最初は速く、だんだんゆっくり近づいて、小さくなる ---
	const float t = std::clamp(stateTimer_ / clearTime_, 0.0f, 1.0f);
	const Vector3 next = MathUtility::Lerp(clearStart_, clearTarget_, Ease::OutQuad(t, 0.0f, 1.0f));
	EntityManager::TranslateWorld(gameObject.handle, next - GetWorldPosition(transform));
	bodyScale_ = 1.0f - t;
}

// ===== 状態切り替え =====
void IceCube::ChangeState(State next) {
	// 緑の壁から離れるとき：その壁を覚えて、Wall Regrab Time 秒はくっつかない
	// （離れたフレームも、前のフレームの当たりで「壁に触れている」が来るので、すぐ戻ってくっつかないように）
	if (state_ == State::Stick && next != State::Stick) {
		lastWall_ = stickWall_;
		wallRegrabTimer_ = wallRegrabTime_;
	}
	// 風に流されるのは Move の間だけ（くっつく・吹っ飛ぶ・やられ・クリアでは、流された速さを捨てる）
	if (next != State::Move) {
		windDriftX_ = 0.0f;
	}
	state_ = next;
	stateTimer_ = 0.0f;
}

// ===== 操作できるか（やられ・クリアの最中でない）=====
bool IceCube::IsPlayable() const {
	switch (state_) {
	case State::Move:
	case State::Stick:
	case State::Launch:
		return true;
	case State::Dead:
	case State::Clear:
		return false;
	}
	return false;
}


//=============================================================================
// 地面
//=============================================================================
void IceCube::UpdateGround(const RigidbodyComponent& rigidbody, float deltaTime) {
	sinceJump_ += deltaTime;
	// isGrounded は前のフレームの物理の結果（Script の Update は物理より前に呼ばれる）
	// 跳んだ直後は、まだ足場に触れていて isGrounded になることがあるので、少しの間は数えない
	const bool isGrounded = rigidbody.isGrounded && sinceJump_ >= kGroundIgnoreAfterJump;
	isJustLanded_ = isGrounded && !isOnGround_;
	if (isGrounded) {
		// --- 着地した瞬間：落ちてきた速さの分だけ、つぶれる ---
		if (isJustLanded_) {
			const float impact = (maxFallSpeed_ > 0.0f) ? std::clamp(-previousVelocityY_ / maxFallSpeed_, 0.0f, 1.0f) : 1.0f;
			stretch_ = -landSquash_ * impact;
		}
		isOnGround_ = true;
		groundJumpTimer_ = coyoteTime_; // 地面にいる間は、いつでも満タン
		hasAirJump_ = false;            // 着地したら、自分のジャンプは地面のジャンプ（コヨーテタイム）の方で持つ
	} else {
		isOnGround_ = false;
		groundJumpTimer_ -= deltaTime; // 足場から落ちても、コヨーテタイムの間は地面のジャンプが残る
	}
}


//=============================================================================
// 当たり判定
//=============================================================================
void IceCube::OnCollisionEnter(const CollisionInfo& collision) { CollectContact(collision); }
void IceCube::OnCollisionStay(const CollisionInfo& collision) { CollectContact(collision); }
void IceCube::OnTriggerEnter(GameObject other) {
	// 触れている輪（水色の円・緑の円。どちらも JumpRing なので、1回で見つかる）。使えるかは、跳ぶときに輪に聞く
	if (other.GetComponent<JumpRing>() != nullptr) {
		touchingOrbs_.push_back(other);
	}
}
void IceCube::OnTriggerStay(GameObject other) { OnTriggerEnter(other); } // 触れている間も、毎フレーム集め直す

// ===== 当たりから、足元・緑の壁・吹っ飛びの行き止まりを集める =====
void IceCube::CollectContact(const CollisionInfo& collision) {
	CollectGround(collision);
	// 緑の壁に、横から触れた（当たりの向きが横向き。斜めの壁も入る。上に乗ったら普通の床）
	if (std::abs(collision.normal.x) > kWallNormalX && collision.gameObject.GetComponent<StickyWall>() != nullptr) {
		wallContact_ = collision.gameObject;
		wallNormal_ = collision.normal;
	}
	// 吹っ飛ぶ向きの先にぶつかった（飛ぶ向きと反対を向いた当たり。床をこすりながら飛ぶだけなら数えない）
	if (state_ == State::Launch && Dot(collision.normal, Normalize(launchVelocity_)) < -kLaunchBlockCos) {
		isLaunchBlocked_ = true;
	}
}

// ===== 足元の向きを集める =====
void IceCube::CollectGround(const CollisionInfo& collision) {
	// 足元の当たり（normal が上向き）のうち、一番上を向いている物を、このフレームの足元の向きにする
	if (collision.normal.y >= kStandCos && collision.normal.y > groundNormal_.y) {
		groundNormal_ = collision.normal;
	}
}

//=============================================================================
// 左右移動
//=============================================================================
void IceCube::UpdateRun(RigidbodyComponent& rigidbody, float deltaTime) {
	const float input = ReadMoveInput();
	// 目標の速さ：入力の向きへ Run Speed ＋ 風で流される速さ（追い風で速く、向かい風で遅く。入力が無ければ風の速さで流される）
	const float targetSpeed = input * runSpeed_ + windDriftX_;
	// 入力があれば加速、無ければ減速（地上と空中で強さが違う）
	// 速さを直接書かずに「近づける」ので、急に向きが変わらない
	float rate = 0.0f;
	if (isOnGround_) {
		rate = (input != 0.0f) ? groundAccel_ : groundDecel_;
	} else {
		rate = (input != 0.0f) ? airAccel_ : airDecel_;
	}
	// --- 足元の向きが分かる地面に立っている：坂に沿った速さだけにする（坂を押し下げる分・跳ね上がる分を消す）---
	if (IsStanding()) {
		// 坂に沿った右向き：足元の向き (nx, ny) を右に 90 度回した (ny, −nx)。平らな床なら (1, 0)
		const Vector3 along = {groundNormal_.y, -groundNormal_.x, 0.0f};
		// 着地した瞬間は、空中での横の速さから始める（物理が「落ちてきた勢い」を坂に沿った向きへ曲げているので、そのまま使うと坂をずり落ちる）
		const float current = isJustLanded_ ? previousVelocityX_ : Dot(rigidbody.velocity, along);
		const float speed = MathUtility::MoveTowards(current, targetSpeed, rate * deltaTime);
		rigidbody.velocity = along * speed;
		return;
	}
	rigidbody.velocity.x = MathUtility::MoveTowards(rigidbody.velocity.x, targetSpeed, rate * deltaTime);
}

//=============================================================================
// ジャンプ
//=============================================================================
void IceCube::UpdateJump(RigidbodyComponent& rigidbody, float deltaTime) {
	// --- 跳ぶ：押した（覚えている）ジャンプがあって、使えるジャンプがあるとき ---
	if (ReadJumpRequest(deltaTime)) {
		// 緑の円の中なら、跳ばずに吹っ飛ぶ（地上でも空中でも、ほかのジャンプより先。自分のジャンプは使わない）
		if (UseLaunchOrb()) {
			return;
		}
		// 使う順番：地上（コヨーテタイムの間も）は地面のジャンプ → 空中は、触れている水色の円 → 緑の壁・緑の円で戻った自分のジャンプ
		// || は左から順に調べて、true が出たらそこで止まる（短絡評価）。なので、円を使えたら、自分のジャンプは使わずに残る
		if (CanGroundJump() || UseJumpOrb() || UseAirJump()) {
			Jump(rigidbody);
		}
	}

	// --- 長押し：上がっている途中でジャンプを離したら、上向きの速さを減らす（すぐ離すと低いジャンプ）---
	if (isJumpRising_) {
		if (rigidbody.velocity.y <= 0.0f) {
			isJumpRising_ = false; // もう落ち始めた（最後まで押していた）
		} else if (!IsJumpPressed()) {
			rigidbody.velocity.y *= jumpCutRatio_;
			isJumpRising_ = false; // 1回のジャンプで1回だけ
		}
	}
}

// ===== ジャンプを頼まれているか（今押した・先行入力で覚えている）=====
bool IceCube::ReadJumpRequest(float deltaTime) {
	// 先行入力：押した瞬間を Jump Buffer 秒だけ覚えておく（着地・円に入る・壁にくっつく少し前に押しても、その瞬間に跳ぶ）
	// 押したフレームは isTriggered で見る（Jump Buffer が 0 でも跳べるように）
	const bool isTriggered = IsJumpTriggered();
	if (isTriggered) {
		jumpBufferTimer_ = jumpBufferTime_;
	}
	const bool isRequested = isTriggered || jumpBufferTimer_ > 0.0f;
	jumpBufferTimer_ -= deltaTime;
	return isRequested;
}

// ===== 触れている水色の円を1つ使う（使えたら true）=====
bool IceCube::UseJumpOrb() {
	// 触れている円のうち、使える物を1つ使う（2つ重なっていても、円ごとに覚えているので、使える方が使われる）
	for (const GameObject& orbObject : touchingOrbs_) {
		JumpOrb* orb = orbObject.GetComponent<JumpOrb>();
		if (orb != nullptr && orb->IsReady()) {
			orb->Use();
			return true;
		}
	}
	return false;
}

// ===== 緑の壁・緑の円で戻った自分のジャンプを使う（使えたら true）=====
bool IceCube::UseAirJump() {
	if (!hasAirJump_) {
		return false;
	}
	hasAirJump_ = false;
	return true;
}

// ===== 跳ぶ =====
void IceCube::Jump(RigidbodyComponent& rigidbody) {
	// 長押ししたときに、ちょうど Jump Height まで上がる速さ
	// 速さ v で真上に投げると、重力 g で v² ÷ 2g だけ上がる（v² = 2gh）→ v ＝ √(2gh)。上がっている間の重力（Gravity）で計算する
	// 下向きの速さは捨てる（落ちている途中で円を使っても、同じ高さだけ上がる）
	rigidbody.velocity.y = std::sqrt(2.0f * gravity_ * jumpHeight_);
	TakeOff();
	isJumpRising_ = true;
}

// ===== 地面を離れる（跳ぶ・ばね。地面のジャンプと先行入力を消す）=====
void IceCube::TakeOff() {
	isOnGround_ = false;
	groundJumpTimer_ = 0.0f; // 地面のジャンプを使った（コヨーテタイムの残りも消す）
	jumpBufferTimer_ = 0.0f; // 覚えていたジャンプも使った
	sinceJump_ = 0.0f;
	stretch_ = jumpStretch_; // 見た目：縦に伸びる
}


//=============================================================================
// 重力
//=============================================================================
void IceCube::UpdateGravity(RigidbodyComponent& rigidbody) const {
	// 重力の強さは、Rigidbody の Gravity Scale（Settings の重力の何倍か）で変える。上がっている間は Gravity、落ちる間は Fall Gravity Multiplier 倍
	const float worldGravity = Length(PhysicsSystem::GetGravity());
	if (worldGravity > 0.0f) {
		const float wanted = (rigidbody.velocity.y > 0.0f) ? gravity_ : gravity_ * fallGravityMultiplier_;
		rigidbody.gravityScale = wanted / worldGravity;
	}
	// --- 地面に立っている間は、重力を切って、足元へ少しだけ押し付ける ---
	// 足場の摩擦は 0 なので、重力のままだと、重力の「坂に沿った分」で坂をずり落ちる（止まっていても）
	// 押し付ける分は、物理が押し戻すときに消える（地面から浮かずに、Is Grounded と足元の当たりが続く）
	// このフレームに跳んだら isOnGround_ が false なので、ここは通らない（跳ぶ速さに混ざらない）
	if (IsStanding()) {
		rigidbody.useGravity = false;
		rigidbody.velocity -= groundNormal_ * kGroundStickSpeed;
	} else {
		rigidbody.useGravity = true;
	}
	// --- 落ちる速さの上限（このフレームの重力は物理が後で足すので、ほんの少し超えることはある）---
	rigidbody.velocity.y = (std::max)(rigidbody.velocity.y, -maxFallSpeed_);
}

//=============================================================================
// 緑の円（吹っ飛び）
//=============================================================================
bool IceCube::UseLaunchOrb() {
	// 触れている緑の円のうち、使える物を1つ使って、その円の線に沿って吹っ飛ぶ（使った円は、出るまで使えない）
	for (const GameObject& orbObject : touchingOrbs_) {
		LaunchOrb* orb = orbObject.GetComponent<LaunchOrb>();
		if (orb != nullptr && orb->IsReady()) {
			orb->Use();
			Launch(*orb);
			return true;
		}
	}
	return false;
}

//=============================================================================
// 緑の壁（くっつく）
//=============================================================================
bool IceCube::TryStick(RigidbodyComponent& rigidbody) {
	// --- 早期リターン：緑の壁に横から触れていない・地面にいる・さっき離れた壁 ---
	if (!wallContact_ || isOnGround_) {
		return false;
	}
	if (wallContact_ == lastWall_ && wallRegrabTimer_ > 0.0f) {
		return false;
	}
	// --- くっつく：止まって、自分のジャンプが戻る ---
	ChangeState(State::Stick);
	stickWall_ = wallContact_;
	stickNormal_ = wallNormal_;
	hasAirJump_ = true;
	groundJumpTimer_ = 0.0f;
	isJumpRising_ = false;
	rigidbody.velocity = {0.0f, 0.0f, 0.0f};
	rigidbody.useGravity = false;
	return true;
}


//=============================================================================
// 緑の円（吹っ飛び）
//=============================================================================
void IceCube::Launch(const LaunchOrb& orb) {
	RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>();
	if (!IsPlayable() || rigidbody == nullptr) {
		return;
	}
	// --- 飛ぶ線を、緑の円から写す（飛んでいる間に円が動いても、線は変わらない）---
	orb.BuildPath(launchPath_);
	if (launchPath_.IsEmpty()) {
		return;
	}
	ChangeState(State::Launch); // 吹っ飛びの最中に別の円の中でジャンプしたら、そっちの線で吹っ飛び直す
	launchStart_ = GetWorldPosition(transform);
	const Vector3 pathStart = launchPath_.Evaluate(0.0f).position;
	launchCenter_ = {pathStart.x, pathStart.y, launchStart_.z}; // 奥行きは変えない
	launchSpeed_ = orb.GetSpeed();
	launchDistance_ = 0.0f;
	launchVelocity_ = CalcLaunchPoint(0.0f).tangent * launchSpeed_;
	launchDuration_ = orb.GetDuration();
	isLaunchEndAtPathEnd_ = orb.IsEndAtPathEnd();
	hasAirJump_ = true; // 自分のジャンプが1回ある（吹っ飛びの途中・後で、1回だけ跳べる）
	isOnGround_ = false;
	groundJumpTimer_ = 0.0f;
	jumpBufferTimer_ = 0.0f; // 吹っ飛ばしたジャンプ（先行入力）で、すぐやめないように
	isJumpRising_ = false;
	rigidbody->velocity = {0.0f, 0.0f, 0.0f};
	rigidbody->useGravity = false;
}

void IceCube::UpdateLaunch(RigidbodyComponent& rigidbody, float deltaTime) {
	stateTimer_ += deltaTime;
	// --- 吹っ飛び中でも、横から緑の壁に当たったら、くっつく ---
	if (TryStick(rigidbody)) {
		return;
	}
	// --- 行き止まり（飛ぶ向きの先にぶつかった）・時間切れ：普通の空中へ（今の速さのまま、重力が戻る）---
	if (isLaunchBlocked_ || stateTimer_ >= launchDuration_) {
		ChangeState(State::Move);
		rigidbody.useGravity = true;
		return;
	}
	// --- ジャンプ：別の緑の円の中なら、そっちの向きで吹っ飛び直す（いつでも）---
	// Launch Jump Lock 秒の後なら、吹っ飛びをやめて跳ぶ（横の速さを Launch Jump Keep だけ残す。自分のジャンプを使う）
	// ReadJumpRequest は毎フレーム呼んで、先行入力を数える（Lock の終わる少し前に押しても、終わった瞬間に跳ぶ）
	// 吹っ飛び始めた円は「使った」ので、円から出るまで UseLaunchOrb には選ばれない
	if (ReadJumpRequest(deltaTime)) {
		if (UseLaunchOrb()) {
			return;
		}
		if (stateTimer_ >= launchJumpLock_ && UseAirJump()) {
			ChangeState(State::Move);
			rigidbody.useGravity = true;
			rigidbody.velocity.x = launchVelocity_.x * launchJumpKeep_;
			Jump(rigidbody);
			return;
		}
	}
	// --- 線の始まりに寄る（Launch Gather Time 秒）→ 線に沿って飛ぶ（重力なし）---
	rigidbody.useGravity = false;
	if (stateTimer_ < launchGatherTime_) {
		// 最初は速く、だんだんゆっくり寄る。位置を直接動かす（速さは 0。物理は壁から押し出すだけ）
		const float t = stateTimer_ / launchGatherTime_;
		const Vector3 next = MathUtility::Lerp(launchStart_, launchCenter_, Ease::OutQuad(t, 0.0f, 1.0f));
		EntityManager::TranslateWorld(gameObject.handle, next - GetWorldPosition(transform));
		rigidbody.velocity = {0.0f, 0.0f, 0.0f};
		return;
	}
	// --- 線に沿って、1フレームに Speed × deltaTime ずつ進む ---
	launchDistance_ += launchSpeed_ * deltaTime;
	// 線の終わりで吹っ飛びをやめる（End At Path End）：線の終わりの向きの勢いのまま、普通の空中へ
	if (isLaunchEndAtPathEnd_ && launchDistance_ >= launchPath_.GetLength()) {
		ChangeState(State::Move);
		rigidbody.useGravity = true;
		rigidbody.velocity = CalcLaunchPoint(launchPath_.GetLength()).tangent * launchSpeed_;
		return;
	}
	const PathCurve::Sample point = CalcLaunchPoint(launchDistance_);
	launchVelocity_ = point.tangent * launchSpeed_;
	// 位置は直接動かさず、「線の上の次の点に、このフレームで着く速さ」を Rigidbody に入れる（物理がぶつかる物から押し出せるように）
	// 床をこすって遅れたら、次のフレームで取り戻す。ただし Speed の kLaunchCatchUp 倍まで（大きく遅れても、飛び出さないように）
	const Vector3 position = GetWorldPosition(transform);
	Vector3 velocity = (Vector3{point.position.x, point.position.y, position.z} - position) / deltaTime;
	const float maxSpeed = launchSpeed_ * kLaunchCatchUp;
	if (Length(velocity) > maxSpeed) {
		velocity = Normalize(velocity) * maxSpeed;
	}
	rigidbody.velocity = velocity;
}

// ===== 吹っ飛ぶ線の、distance（m）進んだ所（線の終わりより先は、まっすぐ伸ばす）=====
PathCurve::Sample IceCube::CalcLaunchPoint(float distance) const {
	// 線の終わりより先は、終わりの向きへまっすぐ伸ばす（線が短くても、Duration の間は飛び続ける）
	PathCurve::Sample sample = launchPath_.Evaluate(distance);
	const float over = distance - launchPath_.GetLength();
	if (over > 0.0f) {
		sample.position += sample.tangent * over;
	}
	sample.tangent = Normalize(Vector3{sample.tangent.x, sample.tangent.y, 0.0f}); // 奥行きには飛ばない
	return sample;
}

//=============================================================================
// 風
//=============================================================================
// ===== 風で流される横の速さ（走る目標に足す）=====
void IceCube::UpdateWindDrift(float deltaTime) {
	// 風の中では、風の横の速さへ Strength で近づける。風の外では、地上・空中の減速で 0 へ戻す（風から出ても、少し勢いが残る）
	// Rigidbody の速さを直接押さないのは、UpdateRun が毎フレーム速さを目標へ近づけていて、押した分がすぐ消されるため
	// （目標の方に足しておけば、入力が無ければ風の速さで流され、向かい風に走ると遅くなる）
	const float rate = wind_.IsBlowing() ? wind_.strength : (isOnGround_ ? groundDecel_ : airDecel_);
	windDriftX_ = MathUtility::MoveTowards(windDriftX_, wind_.velocity.x, rate * deltaTime);
}

// ===== 風で、縦の速さを押す（上向きの風で浮く）=====
void IceCube::ApplyWindLift(RigidbodyComponent& rigidbody, float deltaTime) const {
	// 縦はジャンプと重力だけで決まっていて、毎フレーム目標へ戻されないので、エンジンの押し方（Push）をそのまま使う
	// 横は UpdateWindDrift で足したので、ここでは縦だけ使う
	rigidbody.velocity.y = WindZoneSystem::Push(wind_, rigidbody.velocity, deltaTime).y;
}

//=============================================================================
// HP・やられ・クリア
//=============================================================================
// ===== HP を amount だけ減らして、knockback ではねる（頼むだけ。受けるのは次の Update の頭）=====
void IceCube::Damage(int amount, const Vector3& knockback) {
	if (!IsPlayable()) {
		return; // もうやられ・クリアの最中
	}
	// 同じフレームに何回頼まれても、一番大きいダメージと、一番速いはね返りの1回分にする
	pendingDamage_ = (std::max)(pendingDamage_, amount);
	if (LengthSq(knockback) > LengthSq(pendingKnockback_)) {
		pendingKnockback_ = knockback;
	}
}

// ===== 頼まれたダメージを1回分だけ受けて、はねる =====
void IceCube::ApplyDamage(RigidbodyComponent& rigidbody) {
	const int amount = pendingDamage_;
	const Vector3 knockback = pendingKnockback_;
	pendingDamage_ = 0;
	pendingKnockback_ = {0.0f, 0.0f, 0.0f};
	// --- 早期リターン：頼まれていない・もうやられ・クリアの最中 ---
	if ((amount <= 0 && LengthSq(knockback) == 0.0f) || !IsPlayable()) {
		return;
	}
	// --- HP を減らす。0 になったら砕ける ---
	if (amount > 0) {
		hp_ = (std::max)(hp_ - amount, 0);
		if (hp_ == 0) {
			Kill(DeathType::Shatter);
			return;
		}
		hurtTimer_ = 0.0f; // 白く光る・揺れるのを始める（ひびは UpdateBodyMaterial が HP から決める）
	}
	// --- はねる：くっつき・吹っ飛びはやめて、普通の空中へ ---
	if (LengthSq(knockback) > 0.0f) {
		if (state_ != State::Move) {
			ChangeState(State::Move);
		}
		rigidbody.useGravity = true;
		// はねる向きの速さだけを入れ替える（落ちてきた勢いは消して、決まった速さではねる。横に走る速さは残る）
		const Vector3 direction = Normalize(knockback);
		rigidbody.velocity += direction * (Length(knockback) - Dot(rigidbody.velocity, direction));
		TakeOff();
		isJumpRising_ = false; // ジャンプを離したときの「上向きを半分」が、はねた速さに掛からないように
	}
}

// ===== 一発アウト =====
void IceCube::Kill(DeathType how) {
	if (!IsPlayable()) {
		return; // もうやられ・クリアの最中
	}
	hp_ = 0;
	deathType_ = how;
	BeginEnding(State::Dead);
	// --- やられ方ごとの始まり ---
	switch (how) {
	case DeathType::Shatter:
		SpawnShards(); // かけらを作って、元のキューブを隠す
		break;
	case DeathType::Melt:
		meltAmount_ = 0.0f; // UpdateDead で、だんだん溶ける
		break;
	case DeathType::Fall:
		break;
	}
}

// ===== ゴールに入った（GoalDoor から）。doorPosition へ吸い込まれる（次のレベルへ進むのは GoalDoor）=====
void IceCube::Clear(const Vector3& doorPosition) {
	if (!IsPlayable()) {
		return;
	}
	clearStart_ = GetWorldPosition(transform);
	clearTarget_ = {doorPosition.x, doorPosition.y, clearStart_.z}; // 奥行きは変えない
	BeginEnding(State::Clear);
}

// ===== やられ・クリアを始める（物理で動かすのを止める）=====
void IceCube::BeginEnding(State next) {
	ChangeState(next);
	isJumpRising_ = false;
	// --- その場で止める（重力で落ちない。やられ・クリアの動きは、このスクリプトが位置と大きさで作る）---
	if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) {
		rigidbody->velocity = {0.0f, 0.0f, 0.0f};
		rigidbody->useGravity = false;
	}
}

//=============================================================================
// 見た目（子の Body）
//=============================================================================
void IceCube::UpdateBody(const RigidbodyComponent& rigidbody, float deltaTime) {
	if (!body_) {
		return; // 子の Body が無い（見た目は動かさない）
	}
	// --- 傾き：地上は足元の向きに合わせる。空中は進む向きへ倒れる ---
	float targetAngle = 0.0f; // 度（Z 軸のまわり。画面で左回りが＋）
	if (state_ == State::Move) {
		if (isOnGround_ && groundNormal_.y > 0.0f) {
			// キューブの上 (0, 1) を、足元の向き (nx, ny) へ回す角度
			// Z 軸で θ 回すと (0, 1) は (−sinθ, cosθ) になるので、θ ＝ atan2(−nx, ny)
			targetAngle = std::atan2(-groundNormal_.x, groundNormal_.y) * kRadToDeg;
		} else if (!isOnGround_ && runSpeed_ > 0.0f) {
			// 右へ進むと右へ倒れる（右回り＝マイナス）。走る速さのときに Tilt Angle
			targetAngle = -std::clamp(rigidbody.velocity.x / runSpeed_, -1.0f, 1.0f) * tiltAngle_;
		}
		// 風の中では、ぐらぐら揺れる（目標の傾きに足す。下の指数の近づけ方で、なめらかになる）
		if (wind_.IsBlowing()) {
			windWobbleTime_ += deltaTime;
			targetAngle += std::sin(windWobbleTime_ * kWindWobbleSpeed) * windWobbleAngle_;
		}
	}
	if (state_ == State::Launch) {
		// 吹っ飛び中は、飛ぶ向きへ転がるように回り続ける（右へ飛ぶと右回り＝マイナス、左へ飛ぶと左回り）
		bodyAngle_ -= std::copysign(launchSpinSpeed_, launchVelocity_.x) * deltaTime;
	} else {
		// 360 度回った分は捨てて −180〜180 にする（吹っ飛びで何回も回った後に、逆へ何回も回って戻らないように）
		// std::remainder(x, 360)：x を 360 で割った「一番近い整数」との余り（<cmath>。fmod と違い、−180〜180 になる）
		bodyAngle_ = std::remainder(bodyAngle_, 360.0f);
		// 目標へ指数で近づける（Tilt Speed が大きいほどすぐ。フレームの速さが変わっても同じ動き）
		bodyAngle_ += (targetAngle - bodyAngle_) * (1.0f - std::exp(-tiltSpeed_ * deltaTime));
	}
	// --- 伸び縮みは、だんだん元に戻る（Stretch Time で、残りが約 37% になる）---
	stretch_ *= std::exp(-deltaTime / stretchTime_);

	// --- ダメージを受けた直後は揺れる（傾きに足すだけ。Hurt Shake Time で 0 まで小さくなる）---
	hurtTimer_ += deltaTime;
	float shake = 0.0f;
	if (hurtTimer_ < hurtShakeTime_) {
		shake = std::sin(hurtTimer_ * kHurtShakeSpeed) * hurtShakeAngle_ * (1.0f - hurtTimer_ / hurtShakeTime_);
	}

	// --- 子の Transform へ書く ---
	TransformComponent& body = body_.GetTransform();
	const float size = cubeSize_ * bodyScale_;
	// 溶けるときは、縦がつぶれて、横に広がる（meltAmount_ は溶けていなければ 0）
	const float height = size * (1.0f + stretch_) * (1.0f - kMeltSquash * meltAmount_);       // 縦（伸びると高く、つぶれると低く）
	const float width = size * (1.0f - stretch_ * 0.5f) * (1.0f + kMeltSpread * meltAmount_); // 横・奥行き（縦と反対に、半分だけ）
	body.scale = {width, height, width};
	// つぶれても底が浮かないように、高さが変わった分の半分だけ下げる（伸びたときは上げる）
	body.translation = {0.0f, (height - size) * 0.5f, 0.0f};
	SetEulerAngles(body, {0.0f, 0.0f, bodyAngle_ + shake});
	UpdateBodyMaterial();
}

// ===== ひび・ダメージで白く光る =====
void IceCube::UpdateBodyMaterial() {
	PrimitiveRendererComponent* renderer = body_.GetComponent<PrimitiveRendererComponent>();
	if (renderer == nullptr) {
		return;
	}
	// --- ひび：受けたダメージの回数で、テクスチャを替える（入れていないテクスチャは飛ばして、1つ前の物のまま）---
	const int damageTaken = maxHp_ - hp_;
	uint32_t texture = bodyTexture_;
	if (damageTaken >= 1 && crackSmallTexture_ != 0) {
		texture = crackSmallTexture_;
	}
	if (damageTaken >= 2 && crackLargeTexture_ != 0) {
		texture = crackLargeTexture_;
	}
	renderer->material.textureHandle = texture;
	// --- ダメージを受けた直後は白く光って、Hurt Flash Time で元の色へ戻る ---
	const float flash = (hurtTimer_ < hurtFlashTime_) ? 1.0f - hurtTimer_ / hurtFlashTime_ : 0.0f;
	renderer->material.color = MathUtility::Lerp(bodyColor_, Vector4{1.0f, 1.0f, 1.0f, 1.0f}, flash);
}

//=============================================================================
// 砕ける
//=============================================================================
// ===== かけらを作る（Body と同じ見た目の小さな箱。物理は使わず、ここで飛ばす）=====
void IceCube::SpawnShards() {
	shards_.clear();
	if (!body_) {
		return;
	}
	const PrimitiveRendererComponent* renderer = body_.GetComponent<PrimitiveRendererComponent>();
	if (renderer == nullptr) {
		return;
	}
	const PrimitiveRendererComponent look = *renderer; // かけらの見た目は Body と同じ（色・透け方・テクスチャ）。写しを渡す
	const Vector3 center = GetWorldPosition(body_.GetTransform());
	const float size = cubeSize_ * shardSize_;
	for (int i = 0; i < shardCount_; ++i) {
		Shard shard;
		// 親の無い Entity にする（キューブの子にすると、キューブと一緒に動いてしまう）。Transform はすぐ使える
		shard.object = GameObject::Create("Shard");
		TransformComponent& shardTransform = shard.object.GetTransform();
		// キューブの中の、ばらばらな所から（半径 = キューブの大きさの 0.4 倍の球の中）
		const Vector3 offset = RandomEngine::GetInsideUnitSphere() * (cubeSize_ * 0.4f);
		shardTransform.translation = center + offset;
		shardTransform.scale = {size, size, size};
		SetEulerAngles(shardTransform, RandomEngine::GetVector3({0.0f, 0.0f, 0.0f}, {360.0f, 360.0f, 360.0f}));
		shard.object.AddComponent(look); // 見た目が付くのは次のフレームの頭（AddComponent の予約）
		// 飛ぶ向き：真ん中から外へ ＋ 少し上へ（offset が小さくても、上向きの分で向きが決まる）。速さは Shard Speed の半分〜1倍
		const Vector3 direction = Normalize(offset + Vector3{0.0f, cubeSize_ * 0.6f, 0.0f});
		shard.velocity = direction * (shardSpeed_ * RandomEngine::GetFloat(0.5f, 1.0f));
		shard.velocity.z *= 0.3f; // 横から見るので、奥行きの向きにはあまり飛ばさない
		shard.spin = RandomEngine::GetVector3({-720.0f, -720.0f, -720.0f}, {720.0f, 720.0f, 720.0f});
		shards_.push_back(shard);
	}
	body_.SetActive(false); // 元のキューブを隠す（子の中の箱も一緒に消える）
}

// ===== かけらを飛ばす（重力で落ちながら回って、やり直すまでに小さくなって消える）=====
void IceCube::UpdateShards(float deltaTime) {
	const float life = (restartDelay_ > 0.0f) ? std::clamp(1.0f - stateTimer_ / restartDelay_, 0.0f, 1.0f) : 0.0f; // 1 → 0
	const float size = cubeSize_ * shardSize_ * life;
	for (Shard& shard : shards_) {
		if (!shard.object) {
			continue; // もう消えた（エディタで消したなど）
		}
		shard.velocity.y -= gravity_ * fallGravityMultiplier_ * deltaTime;
		TransformComponent& shardTransform = shard.object.GetTransform();
		shardTransform.translation += shard.velocity * deltaTime;
		Rotate(shardTransform, shard.spin * deltaTime);
		shardTransform.scale = {size, size, size};
	}
}


//=============================================================================
// 入力
//=============================================================================
float IceCube::ReadMoveInput() {
	// InputManager::GetMoveAxis は使わない（W・↑ も「上」として入っていて、斜めで長さ1にそろえるので、右とジャンプ（W）を一緒に押すと横が 0.71 倍になる）
	float input = 0.0f;
	if (InputManager::IsKeyPressed(DIK_D) || InputManager::IsKeyPressed(DIK_RIGHT) || InputManager::IsButtonPressed(GamepadButton::DpadRight)) {
		input += 1.0f;
	}
	if (InputManager::IsKeyPressed(DIK_A) || InputManager::IsKeyPressed(DIK_LEFT) || InputManager::IsButtonPressed(GamepadButton::DpadLeft)) {
		input -= 1.0f;
	}
	input += InputManager::GetLeftStickX(); // デッドゾーンの処理は済んでいる
	return std::clamp(input, -1.0f, 1.0f);
}

bool IceCube::IsJumpTriggered() {
	return InputManager::IsKeyTriggered(DIK_SPACE) || InputManager::IsKeyTriggered(DIK_W) || InputManager::IsKeyTriggered(DIK_UP) || InputManager::IsButtonTriggered(GamepadButton::A);
}

bool IceCube::IsJumpPressed() {
	return InputManager::IsKeyPressed(DIK_SPACE) || InputManager::IsKeyPressed(DIK_W) || InputManager::IsKeyPressed(DIK_UP) || InputManager::IsButtonPressed(GamepadButton::A);
}