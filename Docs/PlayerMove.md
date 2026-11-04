# PlayerMove：プレイヤーの移動と回避ジャンプ（PlayerController）

参照・参考元
- 設計書：`C:\Users\K025G\Downloads\ShooterGameDesign.md`（4章 操作とアクション）
- エンジンの Character Controller：`MyEngine_v1/Docs/Tasks/CharacterController.md`（0.3 使い方・0.5 場面ごとの動作）
- エンジンの Script の書き方：`MyEngine_v1/project/MyEngine/ECS/Script.h`
- フレームの速さに左右されない近づけ方（`1 - e^(-k × dt)`）：Freya Holmér「Lerp smoothing is broken」 https://www.youtube.com/watch?v=LSNQuFEDOyQ

最初の版のコードは本体に直接入れた：`project/Source/Scripts/Player/PlayerController.h`・`.cpp`（直接入れたのはこの1回だけ。7章からは写経）。

---

## 1. やったこと・決めたこと

| 項目 | 内容 |
|---|---|
| 消した物 | 前の Script 全部（BossTest・CameraTest・TimelineTest・UIAnimationTest の7ファイル）と vcxproj・filters の行。git に入っていなかったので、写しを scratchpad に取ってから消した |
| 足した物 | `PlayerController`（Add Component → Gameplay → Player） |
| エンジン | 変えていない（Character Controller をそのまま使う） |
| 確認 | Debug・Development・Release の3構成で、このファイルのコンパイルが通る（警告もなし）。リンクと実行はまだ |

| 決めたこと | 理由 |
|---|---|
| 動かすのは Character Controller。Script は velocity を書くだけ | 坂・段・壁・吸い付き・重力はエンジンの仕事（層を分けた。CharacterController.md の 0.4） |
| **回避はダッシュではなく回避ジャンプ**（入力の向きへ斜め上に跳んで、前に1回転） | 避けた先の空中で反撃射撃を撃つと見栄えがいい（2026-11-04 の仕様変更） |
| 回避ジャンプは地面からだけ（空中ではもう出ない） | 「1回」だけ跳ぶ。続けて跳んで逃げ続けられないように |
| 跳び方は **距離（Dodge Distance）と高さ（Dodge Height）** で決める | 手触りの言葉で調整できる。跳ぶ速さは重力から逆算する（3.1） |
| 重力は Character Controller と同じ値を使う | Gravity Scale を変えても距離と高さはそのまま（速く跳んで速く落ちるだけ）。値が2か所に分かれない |
| 無敵は「跳んでから Dodge Invincible Time 秒」 | 跳んでいる間ずっと無敵だと、空中から反撃し放題になる。着地は狙われる |
| 回避ジャンプ中は向きを変えない | 跳んだ向きが読める。壁に当たったら Character Controller が止める |
| 横の進み方は**カーブ**（Dodge Move Curve）で決める（7章で変更） | 最初の版は横の速さが一定で、出だしに一気に速くなり、着地でガクッと落ちた。カーブなら出だしと終わりの速さを Curve エディタで目で見ながら決められる |
| 状態は enum（`PlayerState`）で持つ | 被弾・反撃・必殺を足すときに bool を増やさず、enum と switch に足していける |
| 1回転は**アニメ**でする（Transform を回さない） | 足元が原点なので、Transform を回すと足元を中心に大きく振り回る。回り方・縮こまり方も Blender で作り込める |
| アニメの長さは、跳んでいる時間に合わせて速さを変える | 距離・高さ・重力を変えても、1回転がちょうど着地で終わる |
| モデルの向きのずれを直す値は持たない | 向きがずれていたら、直す場所はモデル（Blender の書き出し）。Script でごまかさない |

---

## 2. シーンに置く（エディタで）

### 2.1 プレイヤー（Entity の名前：Player）

| Component | 値 | メモ |
|---|---|---|
| Transform | 位置 (0, 0, 0)・回転 0・大きさ 1 | 足元が原点（Player.gltf は靴底が原点） |
| Collider | Shape：**Sphere**、Radius **0.45**、Center **(0, 0.45, 0)** | 体の玉と同じ大きさ。下の端が足元（帽子は当たりに入れない） |
| Character Controller | Slope Limit 45、Step Offset 0.3、Use Gravity オン、**Gravity Scale 3** | Gravity Scale 1（9.8）だと跳んでいる時間が約1秒でふわふわする。3 で約 0.57 秒（3.1） |
| Model Renderer | Model：`Resources/Model/Player/Player.gltf` | |
| Animator | Animation：`Player.gltf`、Clip：00_Idle、Loop オン、Play On Start オン | Script が clipIndex・Loop・Speed を書き換える |
| PlayerController | そのまま | |

Rigidbody は付けない（付けると Rigidbody が動かして、Character Controller は何もしない）。

### 2.2 カメラ

今あるカメラに Camera Follow を付けて Target を Player にする。Ground Snap Y は Character Controller でも効く（Plane が XY のとき。見下ろしの XZ はまだ。CharacterController.md の 3章）。

---

## 3. 解説

### 3.1 回避ジャンプの速さの逆算（StartDodge）

投げ上げの式から、高さ H まで上がる速さと、着地までの時間を出す（重力 g）。

| 値 | 式 | 例（H = 1.2m、g = 9.8 × 3 = 29.4、距離 4m） |
|---|---|---|
| 上向きの速さ | vy = √(2gH) | 約 8.4 m/秒 |
| 着地までの時間 | T = 2 × vy ÷ g | 約 0.57 秒 |
| 横の速さ | 距離 ÷ T | 約 7.0 m/秒 |

速さを書くのは跳んだ瞬間の1回だけ。あとは Character Controller が重力で落とし、地面に着くと Y を 0 にする。坂や段の上へ着地したときは T より早く・遅く着くが、それが自然な動き（アニメの速さは平らな地面の T で決めたまま）。

### 3.2 着地の見分け方（UpdateDodge）

この Script の Update は Character Controller より先に呼ばれる。跳んだフレームはまだ地面にいる扱いなので、着地の確認は次のフレームから（Update の switch で Dodge に入るのが次のフレームから）。天井ですぐ止められても、落ちて地面に着けば Move に戻る（ずっと Dodge のままにならない）。

### 3.3 カメラの前がビュー行列の3列目になる理由（CalcMoveDirection）

ビュー行列は「カメラのワールド行列の逆」。カメラのワールド行列の回転の部分は、1行目＝右・2行目＝上・3行目＝前（このエンジンは行ベクトルで、4行目が位置）。回転の行列の逆は**転置**なので、ビュー行列では**3列目 `(m[0][2], m[1][2], m[2][2])` がカメラの前**になる。見下ろしのカメラは下を向いているので y を捨てて長さを1に戻すと、「画面の奥」の地面の向きになる。

### 3.4 `1 - e^(-k × dt)`（CalcDampFactor）

毎フレーム「残りの差 × t」だけ近づけるとき、t を 0.2 のように固定すると、1秒のフレームの数が多いほど速く近づいてしまう。t = 1 - e^(-k × dt) にすると、2フレームに分けても1フレームでまとめても同じ所に着く（e^(-k × a) × e^(-k × b) = e^(-k × (a + b)) なので）。k = 20 なら約 0.15 秒で残りが 5% になる。

### 3.5 あまり使わない STL

- `std::remainder(x, 360)`：x から 360 の一番近い整数倍を引いた余り（-180〜180）。`std::fmod` は符号が x と同じなので、350 度の差が 350 のまま（大回りする）。`std::remainder` なら -10 になって、近い方へ回る
- `std::lerp(a, b, t)`（C++20 の `<cmath>`）：a + (b - a) × t。エンジンの float の `Lerp` は `MathUtility` の中にあるので、STL の方を使う

---

## 4. 確かめること

| # | やること | こうなれば OK |
|---|---|---|
| 1 | 3構成でビルド | エラー・警告なし（リンクはまだ確かめていない） |
| 2 | Play して WASD・左スティック | 押した向き（画面の奥・右）へ動く。斜めでも速くならない |
| 3 | 向き | 動く向きへなめらかに振り向く。横や後ろを向いて動くなら、モデルの書き出しの向きを直す |
| 4 | Space・A | 入力の向きへ斜め上に跳び、約 4m 先に着地。止まったまま押すと前へ |
| 5 | 跳んでいる間に Space | 出ない（地面からだけ） |
| 6 | 着地してすぐ Space | Dodge Cooldown（0.15 秒）の後に出る |
| 7 | アニメ | 止まると Idle、動くと Run（ゆっくり倒すと足もゆっくり）。回避ジャンプは 08_DodgeJump ができるまでは、モデルに入っている形のまま（5章） |
| 8 | 坂・段・壁 | 坂で止まっても滑らない、段を上る、壁に沿って進む（Character Controller の動き） |
| 9 | 壁へ向かって回避ジャンプ | 壁で止まって落ちる |
| 10 | Stop | 置いた位置・向きに戻る |

値の目安（Inspector で変えて手触りを決める）

| 値 | 初め | 変えると |
|---|---|---|
| Move Speed | 6 | 速く・遅く歩く |
| Acceleration | 20 | 小さいと氷の上のように滑る。着地の後の勢いも長く残る |
| Turn Sharpness | 18 | 小さいとゆっくり振り向く |
| Dodge Distance | 4 | 跳んで進む距離 |
| Dodge Height | 1.2 | 跳ぶ高さ。空中の反撃射撃の高さにもなる |
| Gravity Scale（Character Controller） | 3 | 大きいほど速く跳んで速く落ちる（距離と高さは変わらない） |
| Dodge Invincible Time | 0.35 | 長いほど避けやすい |

---

## 5. アニメ：08_DodgeJump を足す

今の Player.gltf には回避ジャンプのアニメが無い。Script は番号 8 を流すので、足すまでは跳んでいる間、モデルに入っている形のまま（アニメの無いポーズ）になる。

| 項目 | 決まり |
|---|---|
| 名前 | `08_DodgeJump`（今の 00〜07 の後ろ。並びが変わると番号がずれるので、後ろに足す） |
| 動き | 踏み切りで縦に縮む → 伸びて跳ぶ → **体の中心（root の少し上）を軸に前へ1回転**（縮こまる）→ 伸びて着地 → 横につぶれる |
| 回る軸 | 足元ではなく体の中心。Transform は回さないので、回転は骨（body）でつける |
| 長さ | 何秒でもよい（Script が跳んでいる時間に合わせて速さを変える）。作りやすい長さで |
| ループ | しない |
| 位置 | その場で回る（前へ進む・上がるのは Character Controller がする。アニメで root を動かさない） |

02_Dash は使わなくなるが、消すと番号がずれるので残す。

---

## 6. まだやっていないこと（次）

- **ジャスト回避**：回避ジャンプを始めた瞬間に「ジャスト判定の範囲」に入っていたか。ボスの攻撃の形と一緒に作る。PlayerController に「回避ジャンプを始めた」を知らせる仕組みを足す
- **空中の反撃射撃**：3 の状態の反撃は空中で撃つ形になる（回避ジャンプ中の状態を足す）
- **ロックオンと自動攻撃・HP と被弾**
- 普段のカメラ（2人を収める）

---

## 7. 直し：回避ジャンプの横の進み方をカーブにする（2026-11-04・写経）

### 7.1 何が起きていたか

最初の版は、跳んだ瞬間に横の速さを「距離 ÷ 時間」（約 7m/秒）にして、着地まで変えなかった。

```
速さ
 7 ┤■■■■■■■■■■■■■■┐            ← 跳んだ瞬間に 0 → 7（一気に移動）
   │                  │
 0 ┤                  └──────    ← 着地で歩きの処理に戻り、0 へ急に落ちる（ガクッ）
   └──────────────────────── 時間
   跳ぶ              着地
```

速さが一定なので、出だしと着地の2か所で速さが急に変わる。

### 7.2 直し方

横の進み方を**カーブ**で決める。カーブは「跳んでからの時間の割合（0〜1）→ 進んだ距離の割合（0〜1）」。毎フレーム、このフレームに進む分の速さを書く。

```
進んだ割合
 1 ┤              ●●●●       InOutSine（未選択のときの形）
   │          ●●              傾き＝速さ。出だしと終わりは傾きが小さい＝ゆっくり
   │      ●●                  真ん中が一番速い（1回転している最中）
 0 ┤●●●●
   └──────────────── 時間の割合
   0               1
```

| 決めたこと | 理由 |
|---|---|
| カーブは**速さではなく「進んだ割合」** | 最後の値が 1 なら、形をどう変えても進む距離は必ず Dodge Distance になる。速さのカーブだと、形を変えるたびに面積（＝距離）が変わる |
| エンジンの Curve（.curve）を使う | Curve エディタで形を見ながら直せる。ほかの動き（ボスの飛びかかりなど）も同じ作り方にできる。Project ウィンドウの Create → Curve で作ると、直線（0→1）から始まる |
| 未選択なら InOutSine | ファイルを作らなくても、ゆっくり出てゆっくり止まる形で動く |
| 縦は今までどおり Character Controller の重力 | 高さと着地の時間は物理のまま（3.1） |
| 平らな地面の着地の時間を過ぎても落ちているときは、速さを書かない | 台地の端から跳んだとき、最後の速さのまま自然に落ちる（真下に落ちない） |

出だしをもう少し速くしたい（回避らしく）なら、カーブの最初の傾きを上げる。着地の後に勢いを残したいなら、最後の傾きを上げる。

### 7.3 パートA：`PlayerController.h`（4か所）

**A-1** 前方宣言を足す

前：
```cpp
// （坂で止まる・段を上る・壁に沿う・地面に吸い付く・重力は Character Controller の仕事）

```

後：
```cpp
// （坂で止まる・段を上る・壁に沿う・地面に吸い付く・重力は Character Controller の仕事）

// 前方宣言
class Curve;

```

**A-2** Dodge の説明

前：
```cpp
	Dodge, // 回避ジャンプ（入力の向きへ斜め上に跳んで1回転。着地まで向きと横の速さは変えない）
```

後：
```cpp
	Dodge, // 回避ジャンプ（入力の向きへ斜め上に跳んで1回転。着地まで向きは変えない。横の進み方は Dodge Move Curve）
```

**A-3** UpdateDodge・StartDodge の引数（UpdateDodge は速さを書くので const を外す。StartDodge は deltaTime を足す）と、道具を2つ足す

前：
```cpp
	void UpdateDodge(const CharacterControllerComponent& controller, float deltaTime);                         // 回避ジャンプ中・着地
	void StartDodge(CharacterControllerComponent& controller, const Vector3& moveDirection);                   // 回避ジャンプを始める
```

後：
```cpp
	void UpdateDodge(CharacterControllerComponent& controller, float deltaTime);                               // 回避ジャンプ中（横の速さ）・着地
	void StartDodge(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime);  // 回避ジャンプを始める
```

前：
```cpp
	static float CalcGravity(const CharacterControllerComponent& controller); // このキャラにかかる重力の強さ（m/秒²。Character Controller と同じ値）
```

後：
```cpp
	static float CalcGravity(const CharacterControllerComponent& controller); // このキャラにかかる重力の強さ（m/秒²。Character Controller と同じ値）
	float CalcDodgeHorizontalSpeed(float elapsed, float deltaTime) const; // 回避ジャンプで、elapsed 秒から deltaTime 秒の間に横へ進む速さ（カーブから）
	const Curve& GetDodgeMoveCurve() const;                             // 回避ジャンプの横の進み方のカーブ（未選択なら InOutSine）
```

**A-4** Inspector の値を足す

前：
```cpp
	float dodgeHeight_ = 1.2f;            // 回避ジャンプの一番高い所（m。足元から）。空中で反撃射撃を撃つ高さになる
```

後：
```cpp
	float dodgeHeight_ = 1.2f;            // 回避ジャンプの一番高い所（m。足元から）。空中で反撃射撃を撃つ高さになる
	uint32_t dodgeMoveCurve_ = 0;         // 回避ジャンプの横の進み方（.curve の番号。横軸＝跳んでからの時間の割合 0〜1、縦軸＝進んだ距離の割合 0〜1。0 なら InOutSine）
```

### 7.4 パートB：`PlayerController.cpp`（6か所）

**B-1** include を2つ足す

前：
```cpp
#include <MyEngine/Components/Animation/AnimatorComponent.h>
#include <MyEngine/Graphics/Animation/AnimationManager.h>
```

後：
```cpp
#include <MyEngine/Components/Animation/AnimatorComponent.h>
#include <MyEngine/Curve/Curve.h>
#include <MyEngine/Curve/CurveManager.h>
#include <MyEngine/Graphics/Animation/AnimationManager.h>
```

**B-2** Fields に1行足す（Dodge Height の次）

前：
```cpp
	ui.Field("Dodge Height", dodgeHeight_, AtLeast(0.0f).Tip("回避ジャンプの一番高い所（m。足元から）。跳んでいる時間は、これと Character Controller の重力で決まる"));
```

後：
```cpp
	ui.Field("Dodge Height", dodgeHeight_, AtLeast(0.0f).Tip("回避ジャンプの一番高い所（m。足元から）。跳んでいる時間は、これと Character Controller の重力で決まる"));
	ui.AssetField("Dodge Move Curve", dodgeMoveCurve_, AssetType::Curve, Tip("回避ジャンプの横の進み方（.curve）。横軸＝跳んでからの時間の割合 0〜1、縦軸＝進んだ距離の割合 0〜1。未選択なら InOutSine（ゆっくり出て、ゆっくり止まる）"));
```

**B-3** UpdateMove の中の StartDodge の呼び方

前：
```cpp
		StartDodge(controller, moveDirection);
	}
```

後：
```cpp
		StartDodge(controller, moveDirection, deltaTime);
	}
```

**B-4** StartDodge（上の説明の1行・関数の頭・最後の部分）

前：
```cpp
//   横の速さ ＝ 距離 ÷ T
// 重力は Character Controller と同じ値を使うので、Gravity Scale を変えても距離と高さはそのまま（速く跳んで、速く落ちるだけ）
void PlayerController::StartDodge(CharacterControllerComponent& controller, const Vector3& moveDirection) {
```

後：
```cpp
//   横は T の間に Dodge Distance だけ進む。どう進むか（出だし・終わりの速さ）は Dodge Move Curve（CalcDodgeHorizontalSpeed）
// 重力は Character Controller と同じ値を使うので、Gravity Scale を変えても距離と高さはそのまま（速く跳んで、速く落ちるだけ）
void PlayerController::StartDodge(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime) {
```

前：
```cpp
	// 横と上の速さを1回だけ書く。あとは Character Controller が重力で落とし、壁に当たれば壁へ向かう分を消す
	controller.velocity = dodgeDirection_ * (dodgeDistance_ / dodgeAirTime_) + Vector3{0.0f, upSpeed, 0.0f};

	state_ = PlayerState::Dodge;
	dodgeElapsed_ = 0.0f;
}
```

後（`state_`・`dodgeElapsed_` を先にする。CalcDodgeHorizontalSpeed が dodgeAirTime_ を使うので、その計算より後で呼ぶ）：
```cpp
	state_ = PlayerState::Dodge;
	dodgeElapsed_ = 0.0f;

	// 上の速さは1回だけ書く（あとは Character Controller が重力で落とす）。横はこのフレームの分。次のフレームからは UpdateDodge が書く
	controller.velocity = dodgeDirection_ * CalcDodgeHorizontalSpeed(0.0f, deltaTime) + Vector3{0.0f, upSpeed, 0.0f};
}
```

**B-5** UpdateDodge を全部差し替え

前：
```cpp
// ===== 回避ジャンプ中・着地 =====
// 横の速さは書き直さない（跳んだ勢いのまま。壁に当たったら Character Controller が止める）
void PlayerController::UpdateDodge(const CharacterControllerComponent& controller, float deltaTime) {
	dodgeElapsed_ += deltaTime;
	// 地面に戻ったら着地。始めたフレームは Character Controller がまだ動かしていないので、ここに来るのは次のフレームから
	// （跳んだ次のフレームには空中になっている。天井ですぐ止められたときも、落ちて地面に着けばここで終わる）
	if (controller.IsGrounded()) {
		state_ = PlayerState::Move;
		dodgeCooldownTimer_ = dodgeCooldown_;
	}
}
```

後：
```cpp
// ===== 回避ジャンプ中・着地 =====
void PlayerController::UpdateDodge(CharacterControllerComponent& controller, float deltaTime) {
	// --- 着地：地面に戻ったら歩きへ。始めたフレームは Character Controller がまだ動かしていないので、ここに来るのは次のフレームから ---
	// （跳んだ次のフレームには空中になっている。天井ですぐ止められたときも、落ちて地面に着けばここで終わる）
	if (controller.IsGrounded()) {
		state_ = PlayerState::Move;
		dodgeCooldownTimer_ = dodgeCooldown_;
		return;
	}
	// --- 横の速さ：カーブの進み方になるように、このフレームに進む分の速さを毎フレーム書く（縦は Character Controller の重力のまま）---
	// 壁に当たっても毎フレーム同じ向きへ書くが、壁へ向かう分は Character Controller が消すので、壁に沿うか止まる
	// 平らな地面で着地する時間（T）を過ぎてもまだ落ちている（台地の端から跳んだ）ときは書かない＝最後の速さのまま落ちる
	if (dodgeElapsed_ < dodgeAirTime_) {
		const Vector3 horizontal = dodgeDirection_ * CalcDodgeHorizontalSpeed(dodgeElapsed_, deltaTime);
		controller.velocity.x = horizontal.x;
		controller.velocity.z = horizontal.z;
	}
	dodgeElapsed_ += deltaTime;
}
```

**B-6** 道具を2つ足す（「なめらかに近づける割合」の上）

前：
```cpp
// ===== なめらかに近づける割合 =====
```

後：
```cpp
// ===== 回避ジャンプで、elapsed 秒から deltaTime 秒の間に横へ進む速さ =====
// カーブは「時間の割合 → 進んだ距離の割合」。2つの時刻の割合の差 × 距離 が、この間に進む距離。それを時間で割ると速さになる
// 速さではなく「進んだ割合」をカーブにするのは、カーブの形をどう変えても、最後に進む距離がちょうど Dodge Distance になるため
// （速さのカーブだと、形を変えるたびに面積＝距離が変わってしまう）
float PlayerController::CalcDodgeHorizontalSpeed(float elapsed, float deltaTime) const {
	if (dodgeAirTime_ <= 0.0f || deltaTime <= 0.0f) {
		return 0.0f;
	}
	const Curve& curve = GetDodgeMoveCurve();
	const float from = curve.Evaluate(elapsed / dodgeAirTime_);
	const float to = curve.Evaluate((elapsed + deltaTime) / dodgeAirTime_); // 1 を過ぎたら最後の値（Evaluate がそう返す）
	return dodgeDistance_ * (to - from) / deltaTime;
}

// ===== 回避ジャンプの横の進み方のカーブ =====
const Curve& PlayerController::GetDodgeMoveCurve() const {
	if (const Curve* curve = CurveManager::Get(dodgeMoveCurve_)) {
		return *curve;
	}
	// 未選択：ゆっくり出て、ゆっくり止まる形（関数の中の static は、最初に通ったときに1回だけ作られる）
	static const Curve kDefaultCurve = Curve::FromEase(Ease::Type::InOutSine);
	return kDefaultCurve;
}

// ===== なめらかに近づける割合 =====
```

### 7.5 確かめること

| # | やること | こうなれば OK |
|---|---|---|
| 1 | 3構成でビルド | エラー・警告なし（写しで、このファイルのコンパイルは3構成とも通るのを確かめた） |
| 2 | 止まったまま Space | ふわっと出て、真ん中で一番速く、着地の前にゆっくりになる。着地でガクッとしない |
| 3 | 歩きながら Space | 着地の後、歩きの速さへなめらかに戻る |
| 4 | Project → Create → Curve で .curve を作り、Dodge Move Curve に入れる | 直線（0→1）のカーブなら最初の版と同じ一定の速さになる。形を変えると進み方が変わり、進む距離は 4m のまま |
| 5 | 台地の端から外へ跳ぶ | 最後の速さのまま落ちる（真下に落ちない） |
| 6 | 壁へ向かって跳ぶ | 壁で止まって落ちる |

### 7.6 あまり使わない書き方：関数の中の `static`

`static const Curve kDefaultCurve = ...;` を関数の中に書くと、最初にその行を通ったときに1回だけ作られ、プログラムが終わるまで残る（2回目からは作らない）。C++11 から、別々のスレッドから同時に通っても1回だけ作られることが決まっている。毎フレーム `Curve::FromEase` でキーを作り直さないために使った。
