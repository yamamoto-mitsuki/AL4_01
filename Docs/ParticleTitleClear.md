# 設計：光るパーティクル・川瀬式ブルーム・タイトル／ステージ選択・クリア画面（2026-10-25）

参考：
- 川瀬式ブルーム（MGF：Multiple Gaussian Filter）… 川瀬正樹「Frame Buffer Postprocessing Effects in DOUBLE-S.T.E.A.L (Wreckless)」GDC 2003 の講演
- Unity の Particle System（Main・Emission・Shape・Over Lifetime の分け方を手本にした）
- 今のエンジン：`MyEngine_v1/project/MyEngine/Particle/*`・`Graphics/PostEffect/BloomPass.*`・`UI/UIScreen.h`・`Scene/SceneTransition.h`
- 今のゲーム：`project/Source/Scripts/GoalDoor.*`・`LevelInfo.*`・`Gimmicks/JumpRing.*`・`UI/PauseMenu.*`

> **このファイルは設計だけ**（写経用のコードは、決めてほしいこと（9章）が決まってから、パートごとに別の md に書く）。

> ⚠ **エンジンを変える所がある**（2章のパーティクル・3章のブルームとスター）。AL4_01 の CLAUDE.md に「Engine 層は変えない」とあるので、**エンジンの部分は MyEngine_v1 のエンジンのチャットで作る**（10/25 に決めた）。このファイルの2・3章は、エンジンのチャットに渡す設計。ゲームの部分（4〜7章）はエンジンを変えずに作れる。

---

## 0. 結論（先に）

| | 作る物 | 置き場所 | 大きさ | 優先 |
|---|---|---|---|---|
| **A** | 進み具合（どこまでクリアしたか）の保存・ステージの一覧（10個） | ゲーム | 小 | **P1** |
| **B** | メニューの共通部分（`PauseMenu` から取り出す） | ゲーム | 小 | **P1** |
| **C** | タイトル（Start・Quit）→ ステージ選択（10個。クリアしないと次が開かない） | ゲーム（シーン1つ・UI Screen 1つ） | 中 | **P1** |
| **D** | クリア画面（次へ・タイトルへ） | ゲーム（UI Screen 1つ） | 小 | **P1** |
| **E** | **Particle System Component**（エディタで置いて調整できるパーティクル） | **エンジン** | 中 | P1〜P2 |
| **F** | 円・ゴールのパーティクル（中心でキラキラ・使ったら／ゴールしたら飛び散る） | ゲーム（E を使う） | 小 | P2 |
| **G** | **川瀬式ブルーム**（縮めた絵ごとにガウスぼかし → 全部足す）＋**ゴールだけ光らせる層（グロー層）**＋**川瀬式のスター（光芒）** | **エンジン** | 大 | P1〜P2 |

- **エンジン側（E・G）はエンジンのチャットで作る**（エンジンの機能も増やしたいので、全部やる）。ゲーム側（A〜D・F）はこちら。
- 遊ぶ流れ（タイトル → 選ぶ → 遊ぶ → クリア → 次へ）はエンジンを変えずに作れる。E・G と並べて進められる。
- パーティクルは「Component にする」のが本題（E）。今は Component が無く、コードで `ParticleEmitter` を作るしかない（2.1）。
- 川瀬式は G1（全体のブルームを切り替え）→ G2（ゴールだけの層、または HDR）→ G3（スター）の順。

---

## 1. 画面の流れ

```
起動
 └→ Title シーン ─ Start ─→ [StageSelect 画面]（UI Screen。Title の上に重ねる）
      │                        │  ←→↑↓：選ぶ（開いていないステージは選べるが、入れない）
      │                        │  決める：そのステージへ（幕）
      │                        └─ Esc / B：閉じて Title へ戻る
      └ Quit ─→ 終了（エディタでは何もしない。9章 Q5）

Level01〜Level10 シーン
 ├ Esc → [Pause 画面]：つづける / やりなおす / タイトルへ（← 今は「タイトルへ」が空。D で作る）
 └ ゴールの扉に触れる
     → キューブが吸い込まれる＋パーティクルが飛ぶ（F）＋クリアを保存（A）
     → Delay 秒後に [Clear 画面]：次へ / タイトルへ
          次へ   → 次のステージ（最後のステージでは「次へ」を出さない）
          タイトルへ → Title シーン（ステージ選択を開いた状態で戻るかは 9章 Q4）
```

- 切り替えは全部 `SceneTransition::LoadScene`（幕）。
- `main.cpp` の `Engine::Initialize(config, "Level01")` を `"Title"` にする。

---

## 2. パーティクル（エンジン）：Particle System Component — パート E

### 2.1 今のパーティクルの問題

| | 今 | 困ること |
|---|---|---|
| 置き方 | `ParticleEmitter` をコード（シーンのクラス）で作る。Component が無い | **エディタで置けない・調整できない**。円・ゴールの Prefab に入れられない |
| 動き | 速さ ±0.5 のランダム・寿命 1〜3 秒・**色もランダム**・A が線形に減る（`MakeNewParticle` に決め打ち） | 「真ん中でキラキラ」「外へ飛び散る」が作れない。大きさ・重力・形が無い |
| 出し方 | `frequency` 秒ごとに `count` 個 | 「使った瞬間に 30 個」（バースト）が無い |
| グループ | `CreateGroup` は増える一方で消せない（`ClearParticles` は粒だけ消す） | シーンを読み直すたびに、エミッターのグループが増える |
| 持ち方 | 粒が `std::list` | 粒ごとに new。並びが飛び飛びで遅い |

良い所（そのまま使う）：
- **ビルボードは VS でやっている**（`Particle.VS` が `gCamera.right / up` で四角を組む）。Scene ビューでもデバッグカメラを向く。
- 描画は1グループ1回のインスタンス描画（`Renderer::DrawParticle` → `RenderContext::DrawParticles`）。被写界深度の後に描く（ぼけない）。

### 2.2 作る物

**Trail と同じ形**にする：Component は「どう出すか」の設定だけ（memcpy できる型）、粒の列は System が持つ（実行中だけ。保存しない）。

| ファイル | 中身 |
|---|---|
| `Components/ParticleSystemComponent.h`（新） | 設定。Add Component → **Effects → Particle System** |
| `Systems/ParticleSystem.h / .cpp`（新） | 登録・粒の列（Entity ごと）・更新・描画・ゲームから呼ぶ関数 |
| `Graphics/Renderer/Renderer.h / .cpp` | 粒の配列（`std::span<const ParticleData>`）をそのまま描く `DrawParticleInstances` を足す。今の `DrawParticle`（list）は中で配列にして、これを呼ぶ（同じ処理を2回書かない） |
| `Window/WindowManager.cpp`・`Engine.cpp` | `Update`・`Draw`・`Initialize`・`Release` を呼ぶ（`TrailSystem` の隣） |
| Hierarchy の ＋ → Effects → Particle System | あると楽（Arrow Strip と同じ作り） |
| `Docs/Reference.md` 15章 | 書き足す |

> `Components/`（26）・`Systems/`（38）はもう 15 を超えている。今までどおり、分けるのは提出の後（AL4_01Game.md 3章）。

#### Component の項目（Inspector の見出しごと）

```cpp
enum class ParticleShape { Point, Sphere, Circle, Cone, Box }; // 出る場所と向き
enum class ParticleBlend { Add, Normal };                      // 重ね方（Trail と同じ）
enum class ParticleSpace { World, Local };                     // 粒が Entity に付いていくか

struct ParticleSystemComponent {
	bool enabled = true;

	// ===== 再生 =====
	bool playOnStart = true;   // Play で流し始める（false：ゲームが ParticleSystem::Play を呼ぶまで出さない。使ったら飛び散る用）
	bool looping = true;       // Duration 秒ごとにくり返す（false：1回で終わる）
	float duration = 1.0f;     // 1回の長さ（秒）

	// ===== 出す量 =====
	float rateOverTime = 10.0f; // 1秒に出す数（0 でバーストだけ）
	int burstCount = 0;         // 1回の始めに、まとめて出す数
	int maxParticles = 256;     // 上限（ParticleManager::kMaxParticles まで）

	// ===== 出る場所と向き（Shape）=====
	ParticleShape shape = ParticleShape::Sphere;
	float radius = 0.5f;            // Sphere・Circle・Cone の半径
	float coneAngle = 25.0f;        // Cone の開き（度）。向きは Entity の上（+Y）
	Vector3 boxSize = {1.0f, 1.0f, 1.0f};
	bool fromSurface = false;       // 形の表面（縁）から出す（円の輪から飛び散る）

	// ===== 粒の初めの値（Min〜Max の乱数）=====
	Vector2 lifetime = {0.5f, 1.0f}; // 秒
	Vector2 speed = {1.0f, 2.0f};    // 外向き（Shape の向き）の速さ m/s
	Vector2 size = {0.1f, 0.2f};     // m
	Vector4 colorA = {1, 1, 1, 1};   // この2色の間でランダム
	Vector4 colorB = {1, 1, 1, 1};

	// ===== 生きている間（Over Lifetime）=====
	Vector3 gravity = {0.0f, 0.0f, 0.0f}; // m/s²（上へ飛ばすなら +Y）
	float drag = 0.0f;                    // 1秒あたりの減速の割合（0〜。止まっていく火花）
	float endSizeRate = 1.0f;             // 最後の大きさ（初めに掛ける。0 で消えるように縮む）
	Vector4 endColor = {1, 1, 1, 0};      // 最後の色（初めの色に掛ける。A 0 でフェードアウト）
	Ease::Type ease = Ease::Type::Linear; // 大きさと色の変わり方（Easing.h をそのまま）

	// ===== 見た目 =====
	uint32_t textureHandle = 0;          // 0 なら白い四角（丸い粒の PNG を入れる）
	ParticleBlend blend = ParticleBlend::Add;
	ParticleSpace space = ParticleSpace::World;
	bool glow = false;                   // グロー層にも描く（3章の G2 ができてから効く）
};
static_assert(std::is_trivially_copyable_v<ParticleSystemComponent>);
```

- **1つの Entity に Particle System は1つ**（ECS の決まり）。「キラキラ」と「飛び散る」の2つが要る物は、子を2つ作る（2.4）。
- 回転（粒が回る）は入れない：VS が四角を `right / up` で組むので、回すには VS とインスタンスのデータを変える必要がある。要るなら後で。

#### System（ゲームから呼ぶ関数）

```cpp
class ParticleSystem {
public:
	static void Initialize();
	static void Release();
	// 毎フレーム（TrailSystem と同じ所）。停止中は粒を全部捨てる（プレビュー中の1つだけは動かす。下）
	static void Update(bool isGameRunning, float deltaTime);
	static void Draw();

	// ===== ゲームから使う =====
	static void Play(Handle<Entity> entity);                     // 最初から（バーストも出る）
	static void Stop(Handle<Entity> entity, bool clear = false); // 出すのをやめる（clear：出ている粒も消す。false なら寿命まで残る）
	static void Emit(Handle<Entity> entity, int count);          // 今すぐ count 個（Play していなくても出る）
	static bool IsAlive(Handle<Entity> entity);                  // 出している・粒が残っている
};
```

中身（`TrailSystem.cpp` と同じ流れ）：

1. 消えた・外した Entity の粒を捨てる（`std::erase_if`）。
2. `ForEach<ParticleSystemComponent>`：
   - 再生中なら `time += dt`。`time` が 0 を越えた瞬間に `burstCount` 個。毎フレーム `carry += rateOverTime × dt` して、整数の分だけ出す（端数は次のフレームへ持ち越す。フレームの速さで数が変わらない）。
   - `time ≥ duration`：looping なら `time -= duration`（バーストもまた出る）、でなければ出すのをやめる。
   - 粒：`age += dt`、寿命で消す（**後ろと入れ替えて pop**。並びは要らないので速い）、`velocity += gravity × dt`、`velocity *= max(0, 1 − drag × dt)`、`position += velocity × dt`。
3. 粒は `std::vector<Particle>`（Entity ごと）。粒の型は新しく小さく作る（`position・velocity・startColor・startSize・age・lifetime`）。
- **Local**：位置を Entity のローカルで持ち、描くときに Entity のワールド行列を掛ける（動く物に付いていく）。
- 乱数は `RandomEngine`（mt19937）。出る場所は Shape ごとに：Sphere ＝ `GetInsideUnitSphere`、Circle ＝ XY 平面の円（このゲームは横から見るので、円は XY）、Cone ＝ +Y を中心に開き角の中、Box ＝ 箱の中。`fromSurface` なら長さを半径にそろえる。

描画：
- Entity ごとに `ParticleData`（`world` はスケール×平行移動だけ。VS は中心と大きさしか読まない）と色（`lerp(1, endColor, ease(t)) × 初めの色`）を並べて、`Renderer::DrawParticleInstances` を1回。
- `Normal` の重ね方は奥から順に並べないと縁が汚い。**このゲームは Add だけでよい**ので、Normal は並べ替えなし（Inspector の説明に書く）。

#### 停止中のプレビュー（あると調整が楽）

- 今の作りでは Script・粒は再生中しか動かない。Arrow Strip は停止中も流れるので Inspector で調整できた（G7b）。
- Inspector に **Preview** ボタン：押した Particle System を、停止中も止めない時間（`GetUnscaleDeltaTime`）で動かす。選択を外したら止める。1つだけ持つ。

### 2.3 今の ParticleManager / ParticleEmitter はどうするか

- **消さない**（Reference.md 15章・CG3_Project が使っている）。新しい System は ParticleManager を使わず、自分で粒を持って `Renderer` に渡す。
- ついでに直すとよい：シーンを読み直すたびに `CreateGroup` が増える件（2.1）。`ClearParticles` でグループごと消すか、`DestroyGroup` を足す（小さい）。

### 2.4 ゲームでの使い方 — パート F

| 物 | 子に置く Particle System | いつ |
|---|---|---|
| 水色の円（JumpOrb）・緑の円（LaunchOrb） | **Sparkle**：Sphere 半径 0.2・rate 8・speed 0.1〜0.3・寿命 0.6〜1.0・end A 0・Add・円の色 | ずっと（playOnStart）。使ったら止めて、キューブが出たらまた流す |
| 〃 | **Burst**：Circle・fromSurface・burst 24・rate 0・speed 3〜5・drag 2・endSize 0・looping なし | 使った瞬間に `Play` |
| ゴールの扉（GoalDoor） | **Sparkle**：Box（扉の大きさ）・rate 15・gravity +Y 0.5・glow | ずっと |
| 〃 | **Burst**：Sphere・fromSurface・burst 60・speed 4〜8・drag 1.5・gravity +Y 1・glow | ゴールした瞬間に `Play`、Sparkle は `Stop` |

- **円の2つは `JumpRing`（共通の親クラス）に書く**（JumpOrb・LaunchOrb に同じことを2回書かない）：
  - `JumpRing::Use()` の中で、子の `Burst` を `Play`、子の `Sparkle` を `Stop`。
  - `JumpRing::OnTriggerExit`（もう一度使えるようになる所）で `Sparkle` を `Play`。
  - 子はその場で `gameObject.FindChild("Burst")` で探す（使う時だけなので、Start で覚えなくてよい。JumpOrb・LaunchOrb の `Start` を変えずに済む）。子が無ければ何もしない。
- ゴールは `GoalDoor::OnTriggerEnter` の `cube->Clear(...)` の隣で同じことをする。
- Prefab（`JumpOrb.prefab`・`LaunchOrb.prefab`）に子を足せば、全部のステージの円に入る。
- 粒の絵：白い丸のぼかし（256² 透明 PNG）を1枚。色はエンジンで掛ける（AL4_01Game.md 12章の決まり）。

---

## 3. 川瀬式ブルームとゴールだけ光らせる（エンジン）— パート G

### 3.1 今のブルームと川瀬式の違い

| | 今（`BloomPass`） | 川瀬式（MGF） |
|---|---|---|
| 抜き出し | しきい値 0.75＋knee で明るい所 | 同じ |
| ぼかし方 | 1/2 → 1/16 へ**テントで縮めながら**ぼかし、小さい段から**戻しながら足す**（デュアルフィルタ式。4段） | 1/4・1/8・1/16・1/32 の縮小を作り、**それぞれの段で横・縦のガウスぼかし**をしてから、**全部の段を重みを付けて足す** |
| 光り方 | 芯の周りに柔らかく広がる | 小さい段ほど広く・薄く。**芯の強い光と、遠くまで届く薄いにじみが両方出る**（段ごとの重みで形を変えられる） |
| 重さ | 軽い | ガウス（9タップ × 2回）× 段の数。それでも小さい絵なので軽い |

- 「川瀬式」と呼ばれる物は2つある：**① MGF（上の表。ブルームの形）** と ② Kawase Blur（ずらした4点を何回もくり返すぼかし方）。どちらも同じ講演。**今回は ① をやる**（ブルームの見た目が変わるのは ①。② はぼかしを速くする手段）。

### 3.2 G1：全体のブルームを川瀬式に切り替えられるようにする

```
シーンの絵 ─(抜き出し・1/2)→ B0
B0 ─縮める→ L1(1/4) ─横ガウス→ T1 ─縦ガウス→ L1'
L1 ─縮める→ L2(1/8) ─横→ ─縦→ L2'
L2 ─縮める→ L3(1/16) ─横→ ─縦→ L3'
L3 ─縮める→ L4(1/32) ─横→ ─縦→ L4'
合成：シーン ＋ Intensity × (w1·L1' ＋ w2·L2' ＋ w3·L3' ＋ w4·L4')   ← 1回の PS で4枚を読む（Bindless なので楽）
```

| ファイル | 変える所 |
|---|---|
| `BloomPass.h / .cpp` | `BloomMode { Dual, Kawase }`。Kawase 用の縮小（`levels`）と横・縦の作業用（`temp`）の RenderTexture。`Record` を mode で分ける。`Param` に `direction`（横 (1,0)・縦 (0,1)）と段の重み4つ |
| `Shader/Data/Post/BloomGaussian.PS.shader`（新） | 9タップの1方向ガウス（重みはシェーダーに定数で。σ ≒ 2） |
| `Shader/Data/Post/BloomKawaseComposite.PS.shader`（新） | 4段を重み付きで足して、シーンに足す |
| `PostProcess.cpp` | Settings → PostEffect → Bloom に **Mode（0：Dual / 1：Kawase）・Weight1〜4** |

- 縮めるのは今の `BloomDown`（テント）をそのまま使う。
- 中間は今と同じ HDR（`R16G16B16A16_FLOAT`）。
- 見比べられるように、Dual はそのまま残す。

### 3.3 G2：ゴールだけ光らせる層（グロー層）

**問題**：今のブルームは画面全体の「明るい所」が光る。シーンの絵は SDR（0〜1）なので、ゴールだけ強く光らせると、同じくらい明るい物（空・円）も光ってしまう。

**やり方**：光らせたい物だけを、もう1枚の絵（グロー層）にも描いて、**その絵にだけ川瀬式（しきい値なし）を掛けて足す**。

```
1. いつもどおり描く（不透明 → DOF → 半透明・パーティクル）
2. グロー層（黒で消す）に、glow の付いた物だけをもう一度描く
   深度はシーンの物を借りて、比べるだけ（書かない）→ 手前の足場に隠れた所は光らない
3. グロー層に川瀬式（しきい値 0）
4. 合成：シーン ＋ ブルーム ＋ Glow Intensity × グロー層のぼかし
```

| ファイル | 変える所 |
|---|---|
| `RenderStates.h` の `DrawTarget` | `bool glow = false;` を足す（**全部の Config・リクエストが受け継ぐ**ので、1か所で済む） |
| `RenderQueue` | `glow` のリクエストを、グロー層用の一覧にも写して積む。`FlushGlow` |
| `SceneRenderer::RenderWorldForPost` | 半透明の後にグロー層を描く（DOF の後の絵に半透明を描く所と同じく、シーンの深度を借りる） |
| `PostProcess` | 2つ目の `BloomPass`（Kawase・しきい値 0）をグロー層用に持つ。合成の PS に「足す物」を1枚増やす |
| Primitive Renderer・Sprite Renderer・Particle System の Component | `glow` を足して、System が Config に写す |

- グロー層は SDR（`R8G8B8A8_UNORM_SRGB`）にする：**今のパイプライン（PSO）をそのまま使える**（HDR にすると、全部の PSO を HDR 用にも作る必要がある）。強さは合成の `Glow Intensity`（1 より大きくしてよい）で決める。
- ゴールの扉（Primitive Renderer・Additive）とゴールのパーティクルに `glow` を付ける。
- 2つ目の描画で、glow の物はリクエストが2回積まれる。数個なので気にしない。

> **別の案（エンジンの PostEffect.md の予定1）：シーンの絵を HDR にする**。HDR なら色を 1 より明るくできるので、「ゴールの色の強さ（Intensity）を 3 にして、しきい値を 1 にする」だけで、ゴールだけ光る（Unity の Emission と同じ考え）。グロー層は要らなくなるが、全部の PSO の出力形式・トーンマッピング・UI の重ね方が変わる大きな作業。**どちらにするかはエンジンのチャットで決める**（エンジンの機能としては HDR の方が広く使える）。

### 3.4 先にやる順

G1（全体を切り替え）で川瀬式が動くのを確かめる → G2（グロー層）か HDR → G3（スター）。

### 3.5 G3：川瀬式のスター（光芒）

**どんな物か**：明るい点から、**十字・星の形に光の筋が伸びる**効果（カメラのクロスフィルター）。同じ講演（DOUBLE-S.T.E.A.L）の物で、ブルーム（丸いにじみ）と一緒に使う。ゴールの扉・円の真ん中の玉にぴったり。

**しくみ（1本の筋）**：筋の向きへ「点を4つ読んで足す」ぼかしを、**読む間隔を4倍ずつ広げながら3回**くり返す。

```
1回目：間隔 1 ピクセルで 4点 → 筋の長さ 4
2回目：間隔 4 ピクセルで 4点 → 長さ 16（1回目の結果をさらに伸ばす）
3回目：間隔 16 ピクセルで 4点 → 長さ 64
重み：離れた点ほど弱く（減衰 a ≒ 0.9〜0.95 を「離れたピクセル数」乗する）→ 先ほど薄い筋になる
```

- 3回の描画で 64 ピクセルの筋ができる（1回で 64 点読むよりずっと軽い）。これが川瀬式のうまい所。
- **筋の本数**：向きを変えて同じことをする。4本（十字。45° 傾けると×）・6本（雪）・8本。最後に全部を足す。
- **きらめき**：筋の向きの角度を時間でゆっくり回す（ゴールが「キラッ」とする）。
- **色の筋**：回ごとに少し色を変えると、虹色の筋になる（講演の色の付け方）。
- **重さ**：ブルームの抜き出し（1/4 の大きさ）から作るので軽い。描画の回数は「本数 × 3回」＋合成。
- **しきい値はブルームとは別**に持つ（ブルームより高くして、**一番明るい所だけ**に筋を出す。どこにでも筋が出るとうるさい）。グロー層（G2）か HDR があれば、ゴールだけに出せる。
- **詰まりそうな所**：
  - `BloomPass::kMaxPassPerFrame = 32`（1フレームに積める描画の数）。ブルームが1ビューで 8 回、スターは「本数 × 3＋2」回で、Scene・Game の2ビューぶん積むと 32 を超えて assert。StarPass は自分の定数バッファを持つか、上限を上げる。
  - 筋を全部足すのに、筋ごとに RenderTexture を持つ（合成で全部読む）か、足し込みのブレンドの PSO を1つ作る（今のブルームの PSO はブレンドなし）。
  - 1回目は抜き出した絵、2・3回目は前の回の結果を読む（行き来の2枚）。読む絵と書く絵を同じにしない。

| ファイル | 変える所 |
|---|---|
| `Graphics/PostEffect/StarPass.h / .cpp`（新） | 筋1本ぶんの作業用 RenderTexture 2枚（行き来）＋全部の筋を足す1枚。本数・長さ・減衰・角度・回る速さ・色の強さ・しきい値 |
| `Shader/Data/Post/StarStreak.PS.shader`（新） | 4点を向きと間隔で読んで、減衰の重みで足す |
| `Shader/Data/Post/StarComposite.PS.shader`（新）か、ブルームの合成に足す | 筋を足す |
| `PostProcess.cpp` | Settings → PostEffect → **Star**（Enabled・Count・Length・Attenuation・Angle・RotateSpeed・Threshold・Intensity・Chromatic） |

---

> **4〜7章（タイトル・ステージ選択・クリア画面）は [OutGame.md](OutGame.md) に詳しく書き直した（2026-10-26）。ずれたら OutGame.md が正。**

## 4. ステージの一覧と進み具合 — パート A

### 4.1 ステージの一覧（ゲームのコードに決め打ち）

```cpp
// Source/Scripts/Progress/StageCatalog.h
struct StageEntry {
	std::string_view scene; // シーンの名前（resources/scenes/<scene>.scene.json）
	std::string_view title; // ステージ選択・クリア画面に出す名前
};
inline constexpr std::array<StageEntry, 10> kStages = {{
    {"Level01", "はじまりの森"},
    {"Level02", "…"},
    // …Level10 まで
}};
```

- **今の `LevelInfo` の Next Scene（シーンごとに次を書く）はやめて、この一覧の順番で次を決める**。10個のシーンに次の名前を打つより間違えにくい・並べ替えが1か所。
- `LevelInfo` の `Number`（1〜10）が一覧の何番目か。番号とシーンの名前が一覧と違ったら、`Start` で警告を出す（打ち間違いに気づく）。
  - シーンの名前から番号を探さないのは、エディタの Play 中に `LoadScene` で切り替えても、`GetOpenSceneName` はエディタで開いたシーンのままなので。
- `LevelInfo::LoadNextLevel` は「一覧の次」へ。最後なら呼ばない（クリア画面が「次へ」を出さない）。
- 場所の名前（Area Name）は残す。

### 4.2 進み具合の保存

```cpp
// Source/Scripts/Progress/GameProgress.h
class GameProgress {
public:
	static bool IsCleared(size_t index);
	static bool IsUnlocked(size_t index); // 0 番はいつも。ほかは1つ前をクリアしたら
	static void MarkCleared(size_t index); // クリアした瞬間に呼ぶ。すぐファイルに書く
	static void UnlockAll();  // デバッグ用
	static void Reset();      // デバッグ用
private:
	static void LoadIfNeeded(); // 初めて読まれた時に1回ファイルを読む
	static inline std::array<bool, kStages.size()> cleared_{};
	static inline bool isLoaded_ = false;
};
```

- ファイル：`saves/Progress.json`（nlohmann。エンジンの externals にあるのを使う）。中身は `{"version":1, "cleared":["Level01","Level02"]}`。**番号ではなくシーンの名前で書く**（一覧の順番を入れ替えても、クリアした物がずれない）。
- 開くのは「1つ前をクリア」だけで決める（クリアした数を持たない）。後で「ベストタイム」を足すなら、ここに足す。
- **エディタと製品版で同じファイル**を読む。エディタで試すとき困るので、タイトルで **F1 ＝全部開く・F2 ＝全部消す**（`#ifdef USE_IMGUI` の中だけ）。
- `cleared_` はシーンの外にあるので Play → Stop でも残るが、ファイルと同じ中身なので問題ない。

---

## 5. メニューの共通部分 — パート B

`PauseMenu` の「項目を集める・選ぶ・見本の飾りを写す・決める」は、タイトル・ステージ選択・クリア画面でも同じ。**4回書かないように、親クラス `MenuBase` に取り出す**（`JumpRing` と同じく SCRIPT で登録しない）。

```cpp
class MenuBase : public Script {
public:
	void Update() final;          // 選ぶ・決める・閉じる（派生では書かない）
	void Fields(ComponentUI& ui) override; // 共通の Screen Name・Style Name・Columns
protected:
	virtual void Decide(const std::string& itemName) = 0; // 決めた（名前で何をするか）
	virtual void OnCancel() {}                // Esc / B
	virtual bool CanDecide(int index) const { return true; } // 決めてよいか（開いていないステージ）
	virtual void OnSelect(int index) {}       // 選んでいる項目が変わった（下に説明を出す・カーソルを動かす）
	virtual bool IsInputBlocked() const { return false; } // 入力を受けない（上に別の画面が開いている）
	int columns_ = 1;            // 1：縦の並び（↑↓）／2 以上：格子（←→ で1つ、↑↓ で columns_ 飛ぶ）
	bool isDecided_ = false;     // 決めた（幕が閉じる間に、もう一度決めない）
	// TextStyle・CollectItems・ApplyLook・selected_・originalStyles_ は今の PauseMenu から移す
};
```

- `CollectItems` は**有効な（SetActive が true の）子だけ**集める → クリア画面の最後のステージで「次へ」を消すと、選べなくなる。
- 派生：`PauseMenu`（Resume・Restart・**Title**）・`TitleMenu`（Start・Quit）・`StageSelectMenu`（Stage01〜10）・`ClearMenu`（Next・Title）。
- PauseMenu の「Title」は今 `return` だけ → `SceneTransition::LoadScene("Title")`。

---

## 6. タイトルとステージ選択 — パート C

### 6.1 Title シーン（`resources/scenes/Title.scene.json`）

| Entity | 中身 |
|---|---|
| Camera | Level01 と同じ森の背景が見える位置。Camera Follow は付けない |
| 背景 | Level01 の背景の板（Prefab の BackGround）・Fog・Depth Of Field を写す。円の Prefab を1つ飾りに置くと、パーティクルも見せられる |
| TitleUI | `TitleMenu`。子に Logo（Text：ゲームのタイトル）・Items（Start・Quit の Text）・SelectedStyle（見本の Text） |

- `TitleMenu`：
  - Start → `UIScreen::Open("StageSelect")`。
  - Quit → 9章 Q5。
  - `IsInputBlocked()` ＝ `UIScreen::IsOpen("StageSelect")`（ステージ選択が開いている間、タイトルの項目は動かない）。
- Level のシーンではないので、`LevelInfo` は置かない（Esc でポーズしない）。

### 6.2 StageSelect 画面（`resources/ui/StageSelect.uiscreen`）

```
┌──────────────────────────────────────┐
│           ステージをえらぶ            │
│   [ 1 ]  [ 2 ]  [ 3 ]  [ 4 ]  [ 5 ]   │  ← 5列 × 2段
│   [ 6 ]  [🔒]  [🔒]  [🔒]  [🔒]   │
│        はじまりの森  ✓クリア          │  ← 選んでいる所の名前
│   ←→↑↓ えらぶ  Enter きめる  Esc もどる │
└──────────────────────────────────────┘
```

| Entity | 中身 |
|---|---|
| StageSelect（一番上） | UI Screen（**Pause Game は外す**。タイトルの背景・パーティクルは動いたまま）・`StageSelectMenu`（Columns 5） |
| Dim | 半透明の黒の Image |
| Items → Stage01〜Stage10 | 1つずつ：Text（番号）。子に Frame（Image）・Lock（Image：鍵）・Check（Image：クリア印） |
| StageName | 選んでいるステージの名前（Text） |
| SelectedStyle | 見本の Text |

- **マスはエディタで10個並べる**（G9b の UI の編集で、ドラッグで置ける）。コードで並べると、見た目の調整がエディタでできない。
- `StageSelectMenu`：
  - `Start` で、マスごとに `GameProgress` を見て Lock・Check の `SetActive` と、番号の色（開いていないマスは暗く）を決める。
  - `OnSelect`：`StageName` の文字を `kStages[i].title`（開いていなければ「？？？」）。
  - `CanDecide`：`GameProgress::IsUnlocked(i)`。開いていないマスで決めたら、鍵を少し揺らす（＋効果音）。
  - `Decide`：名前「Stage03」→ 番号 3 → `SceneTransition::LoadScene(kStages[2].scene)`。
  - `OnCancel`：`UIScreen::Close("StageSelect")`。
  - 最初に選ぶのは「開いている中で一番先」（続きからすぐ遊べる）。

---

## 7. クリア画面 — パート D

### 7.1 流れ（`GoalDoor` を変える）

```
OnTriggerEnter（キューブが触れた）
  → cube->Clear(扉の位置)               … 今のまま
  → Burst を Play・Sparkle を Stop      … パート F
  → GameProgress::MarkCleared(番号 − 1) … ここで保存（クリア画面で何を選んでも、クリアは残る）
Update：Delay（1.0 秒）たったら
  → UIScreen::Open("Clear")             … 今の LoadNextLevel の代わり
```

### 7.2 Clear 画面（`resources/ui/Clear.uiscreen`）

| Entity | 中身 |
|---|---|
| Clear（一番上） | UI Screen（**Pause Game は外す**：止めるとゴールのパーティクルも止まるので。キューブはもう Clear の状態で、操作を受けない）・`ClearMenu` |
| Dim | 半透明の黒 |
| Title | 「ステージ 3 クリア！」（Text。`ClearMenu::Start` で番号と名前を入れる） |
| Items → Next・Title | 「つぎへ」「タイトルへ」 |
| SelectedStyle | 見本 |

- `ClearMenu`：
  - `Start`：最後のステージなら Items の Next を `SetActive(false)`（「タイトルへ」だけ）。
  - Next → `LevelInfo::Find()->LoadNextLevel()`（一覧の次。幕）。
  - Title → `SceneTransition::LoadScene("Title")`。
  - `OnCancel` は何もしない（閉じてゲームに戻る所が無い）。
- クリア画面が出ている間に Esc を押すと、LevelInfo がポーズを開いてしまう → `LevelInfo::Update` で「Clear が開いている間は Esc・R を見ない」。

---

## 8. ファイルの一覧

### ゲーム（AL4_01）

| ファイル | 新／変 | パート |
|---|---|---|
| `Source/Scripts/Progress/StageCatalog.h` | 新 | A |
| `Source/Scripts/Progress/GameProgress.h / .cpp` | 新 | A |
| `Source/Scripts/UI/MenuBase.h / .cpp` | 新（PauseMenu から移す） | B |
| `Source/Scripts/UI/PauseMenu.h / .cpp` | 変（MenuBase の派生に。Title を作る） | B |
| `Source/Scripts/UI/TitleMenu.h / .cpp` | 新 | C |
| `Source/Scripts/UI/StageSelectMenu.h / .cpp` | 新 | C |
| `Source/Scripts/UI/ClearMenu.h / .cpp` | 新 | D |
| `Source/Scripts/LevelInfo.h / .cpp` | 変（Next Scene をやめて一覧で。Clear 中は Esc・R を見ない） | A・D |
| `Source/Scripts/GoalDoor.h / .cpp` | 変（保存・クリア画面・パーティクル） | D・F |
| `Source/Scripts/Gimmicks/JumpRing.h / .cpp` | 変（Burst・Sparkle） | F |
| `main.cpp` | 変（最初のシーンを Title に） | C |
| `Resources/scenes/Title.scene.json` | 新（エディタで） | C |
| `Resources/scenes/Level03〜Level10.scene.json` | 新（エディタで） | — |
| `Resources/ui/StageSelect.uiscreen`・`Clear.uiscreen` | 新（エディタで） | C・D |
| `Resources/prefabs/JumpOrb・LaunchOrb.prefab`・各 Level の Goal | 変（子に Sparkle・Burst） | F |
| `Resources/Textures/Effects/particle_soft.png`・鍵・クリア印 | 新（素材） | C・F |

### エンジン（MyEngine_v1 の方で）

| ファイル | パート |
|---|---|
| `Components/ParticleSystemComponent.h`・`Systems/ParticleSystem.h / .cpp`（新） | E |
| `Renderer.h / .cpp`（`DrawParticleInstances`）・`WindowManager.cpp`・`Engine.cpp`・`MyEngine_v1.vcxproj` | E |
| `BloomPass.h / .cpp`・`PostProcess.cpp`・`BloomGaussian.PS`・`BloomKawaseComposite.PS`（新） | G1 |
| `RenderStates.h`（`DrawTarget::glow`）・`RenderQueue`・`SceneRenderer.cpp`・Primitive / Sprite の Component と System | G2 |
| `StarPass.h / .cpp`・`StarStreak.PS`・`StarComposite.PS`（新）・`PostProcess.cpp` | G3 |
| `Engine::RequestQuit()`（あれば。タイトルの Quit 用） | — |
| `Docs/Reference.md`（15章・ポストエフェクト） | E・G |

---

## 9. 決めてほしいこと

| | 質問 | おすすめ |
|---|---|---|
| ~~Q1~~ | エンジンの部分（E・G）を作るのは、MyEngine_v1 のリポジトリの方でよいか | **決まり（10/25）**：エンジンのチャットで作る |
| Q2 | 「Start, Quad」は **Start・Quit**（ゲームを終わる）でよいか | Quit |
| Q3 | ステージの名前（10個）。今 Level01・02 があるので、あと8つ作る | 名前は後で。初めは「ステージ 1」〜 |
| Q4 | クリア画面の「タイトルへ」で戻ったとき、**ステージ選択を開いた状態**にするか（すぐ次を選べる） | 開いた状態（全体の入れ物に「戻ったらステージ選択を開く」を1つ持つ） |
| Q5 | Quit をエディタで押したとき | 何もしない（ログだけ）。製品版は自分のウィンドウに `WM_CLOSE` を送る（エンジンの `WM_CLOSE` の処理で `DestroyWindow` → ループが終わる。エンジンを変えずに済む）。エンジンに `Engine::RequestQuit()` を足す方がきれいだが、それはエンジンの変更 |
| Q6 | ゴールだけ光らせるのは、グロー層（G2）と HDR のどちらにするか | エンジンのチャットで決める（3.3） |
| Q7 | クリア画面に「もう一度」も入れるか | 入れない（言われた2つ。ポーズに「やりなおす」がある） |

---

## 10. 作る順番

ゲーム（こちら）とエンジン（エンジンのチャット）を並べて進める。

| | ゲーム（AL4_01） | エンジン（MyEngine_v1） |
|---|---|---|
| 1 | **A・B**（一覧・保存・MenuBase。PauseMenu を移して、今までどおり動くのを確かめる） | **E**（Particle System） |
| 2 | **C・D**（Title・StageSelect・Clear・GoalDoor・main.cpp）→ タイトルから通しで遊べる | **G1**（川瀬式ブルーム） |
| 3 | **F**（円・ゴールのパーティクル。E の後） | **G2 か HDR**（ゴールだけ光る） |
| 4 | Level03〜10 | **G3**（スター） |

- A〜D はエンジンを変えないので、エンジンの作業を待たずに進められる。F は E の後。
- エンジンのサブモジュールを新しくしたら、ゲームのビルドが通るか確かめる（API の名前が設計と変わったら、こちらの写経 md を合わせる）。
- 毎日の終わりに、その日の物で通しで遊べる形にしておく（AL4_01Game.md 13章と同じ）。
