# Game.md（Project InkSlash）を作るのに、エンジンに足りない物（2026-10-02 時点）

**Game.md の仕様を、今のエンジンでどう作るか・何が足りないかをまとめた。** API の書き方はエンジンの `MyEngine_v1/Docs/Reference.md`、設計の理由は `MyEngine_v1/Docs/ARCHITECTURE.md`（このリポジトリの隣に置いたエンジンのリポジトリ）。

> **調べたのは MyEngine_v1 の `develop`（9962b81）。** AL4_01 は CG3_Project と同じく、隣の `../MyEngine_v1` を直接参照するので、エンジンは常に最新が使われる。

---

## 0. 結論（先に）

1. **大部分はそのまま作れる。** 入力（WASD・マウス・Game ビューのマウス位置）・Collider と当たりの一覧・Bloom・パーティクル・Camera Component・音は今のエンジンで足りる。
2. **エンジンに足す物は5つ（必須）＋3つ（あると楽）。** どれも小〜中で、合わせて 3〜5 時間ほど。
3. **一番大事なのは G1（タイムスケール）。** 今は `Time::SetTimeScale(0.1f)` を呼んでも、**次のフレームの頭でエンジンが 1 に戻す**ので、Game.md の「スロー」も「ヒットストップ」も効かない（バグに近い）。

| | 足す物 | 大きさ | 要るか | Game.md のどこで使うか |
|---|---|---|---|---|
| **G1** | **ゲームのタイムスケール＋ヒットストップ**（ポーズと分ける） | 小 | **必須** | 3-1 ドラッグ中のスロー、[17〜20h] ヒットストップ |
| **G2** | **マウス位置 → ワールドのレイ**（`Camera::ScreenPointToRay`） | 小 | **必須**（ゲームで書いても可） | 4-A スクリーン→ワールド変換 |
| **G3** | **Collider へのレイキャスト**（`CollisionSystem::Raycast`） | 小〜中 | **必須** | 3-2 壁で線を止める |
| **G4** | **スイープ判定**（線分・カプセルと Collider の重なりを、その場で調べる） | 中 | **必須** | 4-C 高速移動中の斬撃（すり抜け対策） |
| **G5** | **自由な形のメッシュを描く**（`Renderer::DrawMesh`） | 小 | **必須** | 4-D 軌跡の帯（Ribbon Mesh） |
| G6 | 正射影カメラ（Camera Component に Orthographic） | 小 | あると楽 | 1 の「正射影 または 低FOV」 |
| G7 | 線分と点の距離などの数学（`ClosestPointOnSegment`） | 小 | あると楽（ゲームで書いても可） | 4-D インク回収の判定 |
| G8 | 壁の押し戻し（XZ 平面。WASD で壁に入らない） | 小〜中 | あると楽（ゲームで書いても可） | WASD 移動 |

---

## 1. 今のエンジンでそのまま使える物

| Game.md で要る物 | 今のエンジン | メモ |
|---|---|---|
| WASD 移動 | `InputManager::GetMoveAxis()`（WASD・矢印・スティック。斜めでも長さ1） | Y を Z に読み替えるだけ |
| 左クリック長押し・離す | `IsMouseButtonTriggered(0)` / `Pressed(0)` / `Released(0)` | エディタでも Game ビューの上なら ImGui に取られない |
| マウスの画面上の位置 | `GetMousePositionInGame()`（真ん中0・端±1・上が+） | **そのまま NDC（-1〜1）として使える**（カメラの縦横比も Game ビューも 16:9 固定） |
| 見下ろしカメラ | Camera Component（Transform で真上・斜め上から） | 低 FOV（20〜30度）で正射影っぽく見せられる。本当の正射影は G6 |
| 敵・プレイヤーの当たり | Collider（球・箱）＋ `CollisionSystem::GetHits` | 通常移動中のインク回収・敵との接触はこれで足りる |
| 敵の状態（HP・倒された） | ゲームの Component（`COMPONENT` / `SYSTEM`） | Reference.md 5・6章 |
| インクゲージなど全体の値 | ゲームに1つの入れ物（`inline GameState gGameState;`） | Reference.md 6章 |
| 爆散インク（地面の丸い染み） | `Renderer::DrawQuad3d`（テクスチャ・色・ブレンド） | 地面とちらつく（Z ファイティング）ので、y を 0.01 ほど浮かせる |
| 爆散の飛び散り | `ParticleManager`（1グループ 4096 個・加算） | `Initialize` でグループを作る |
| 発光・ネオン | Bloom（しきい値 0.75。Settings → PostEffect で調整） | 色が 0〜1 までなので、**明るい色（彩度の高い原色）にすれば光る**。もっと光らせたいときは Threshold を下げる |
| インクゲージの表示 | `Renderer::DrawSprite`（コードで座標を渡す） | 文字（フォント）は無いので、数字は画像で |
| 効果音・BGM | Audio Source・`AudioSystem::PlayOneShot` | 斬った音・爆散音 |
| 調整する値（スロー倍率・速さ） | `GlobalVariables`（Settings ウィンドウ） | |
| カメラの揺れ | Camera の Entity の Transform を SYSTEM で動かす | エンジンは要らない |

---

## 2. 足す物（必須）

### G1. ゲームのタイムスケール＋ヒットストップ — 小・**必須**

**今の問題：** `MyEngine_v1/MyEngine/Scene/SceneManager.cpp:141` で、**毎フレーム `IScene::Update` の直前に**

```cpp
Time::SetTimeScale(playState_ == PlayState::Paused ? 0.0f : 1.0f);
```

を呼んでいる。ポーズを「時間を0にする」で作っているため、**ゲームが入れた 0.1 は次のフレームで 1 に戻る**。SYSTEM に渡る `deltaTime` は戻った後の値で計算されるので、スローは1フレームも効かない（パーティクルだけ1フレーム遅く効く）。

> Tasks/ActionGame.md の1章に「ヒットストップ：`Time::SetTimeScale` そのまま」とあるが、上の理由で今は効かない。そちらのゲームにも関わる。

**直し方（案）：** 掛け算を2つに分ける。

| 値 | 誰が書くか | 中身 |
|---|---|---|
| `pauseScale` | エンジン（SceneManager） | ポーズ中 0、それ以外 1 |
| `gameScale` | **ゲーム** | スロー（0.1）など。Play / Stop / Restart・シーン切り替えで 1 に戻す |
| ヒットストップ | ゲーム（`Time::HitStop(0.08f)`） | 残り秒を**掛かっていない時間**で減らし、残っている間は 0 |

```cpp
// 使う側（ゲーム）
Time::SetGameTimeScale(0.1f);  // ドラッグ開始
Time::SetGameTimeScale(1.0f);  // ドラッグ終了
Time::HitStop(0.08f);          // 撃破の瞬間（重ねて呼んだら長い方）

// GetDeltaTime() = deltaTime × pauseScale × gameScale × (ヒットストップ中なら 0)
```

- **Stop で 1 に戻すのを忘れない**（スロー中に Stop すると、エディタまでスローのままになる）。
- UI の演出・ドラッグ中のカーソルの点滅など「スローにしたくない物」は `GetUnscaleDeltaTime()`。
- 音のピッチをスローに合わせるかは好み（今回は合わせなくてよい）。

### G2. マウス位置 → ワールドのレイ — 小・**必須**（ゲームで書いても可）

Game.md 4-A。材料（`Inverse`・`GetViewProjectionMatrix`・`Ray`・`Plane`・`Collision::RayPlane`・`GetMousePositionInGame`）は全部ある。**組み立てる関数だけが無い**。Unity の `Camera.ScreenPointToRay` と同じ物なので、エンジンに置く。

```cpp
// Camera.h に足す
// ndc：GetMousePositionInGame() の値（真ん中0、端±1、上が+）
Ray Camera::ScreenPointToRay(const Vector2& ndc) const;
```

中身：NDC の近い点 (x, y, 0) と遠い点 (x, y, 1) を `Inverse(View × Projection)` で戻し、**w で割って**から差を正規化。エンジンの行列は行ベクトル（`m[3]` が平行移動）なので、掛ける向きに注意。

使う側（地面 y=0 との交点）：

```cpp
const Ray ray = CameraSystem::GetMainCamera()->ScreenPointToRay(InputManager::GetMousePositionInGame());
float t = 0.0f;
if (Collision::RayPlane(ray, Plane{{0.0f, 1.0f, 0.0f}, 0.0f}, &t)) {
	const Vector3 ground = Collision::RayPoint(ray, t);
}
```

### G3. Collider へのレイキャスト — 小〜中・**必須**

Game.md 3-2「直前の点から次の点へレイを飛ばし、壁に当たったら止める」。今の `CollisionSystem` は**「重なりの一覧」を作るだけで、その場で「この線は何に当たるか」を聞けない**。形どうしの関数（`Collision::RayAABB` / `RaySphere`）はあるので、Collider の一覧に向けて回す関数を足す。

```cpp
struct RaycastHit {
	Handle<Entity> entity; // 当たった Entity
	Vector3 point;         // 当たった位置
	float distance;        // 始点からの距離
};

// maxDistance までで一番近い当たり。T を持つ Entity だけ（レイヤーの代わり）
template<class T>
static bool CollisionSystem::Raycast(const Ray& ray, float maxDistance, RaycastHit* hit);
```

- **レイヤーが無い**ので、「`Wall` Component を持つ物だけ」を型で絞る（Reference.md の「相手は持っている Component で見分ける」と同じ考え）。
- Collider のワールドの形は `MyEngine_v1/MyEngine/Systems/CollisionSystem.cpp` の中の `MakeWorldCollider` がもう作っているので、それを使い回す。
- **その場で調べる**（前のフレームの一覧ではない）。ドラッグ中に何回呼んでもよい。
- 壁は箱（AABB）なので、回した壁は作れない（今の Collider と同じ制限）。ステージは軸にそろえた箱で組む。

### G4. スイープ判定（線分・カプセル）— 中・**必須**

Game.md 4-C「1フレーム前の位置から今の位置へのカプセルで判定」。**超高速移動だと1フレームで敵を飛び越える**ので、今の Collider（フレームの最後に1回だけ重なりを調べる）では**すり抜ける**。

足す物：

```cpp
// Collision.h（形どうし）
bool CapsuleSphere(const Capsule& capsule, const Sphere& sphere);
bool CapsuleAABB(const Capsule& capsule, const AABB& aabb); // 箱を半径ぶん広げた箱と線分で近似してよい

// CollisionSystem.h（Collider の一覧に向けて）
// 線分 start→end を半径 radius で掃いたときに触れた、T を持つ Entity を全部
template<class T>
static void SweepSphere(const Vector3& start, const Vector3& end, float radius, std::vector<Handle<Entity>>& out);
```

- 1回の斬撃で同じ敵に何度も当たらないよう、「このダッシュで斬った」印（タグ Component か、ダッシュごとの番号）をゲーム側で持つ。
- **代わりの手（エンジンを触らない）：** 高速移動を1フレームに数回（例：0.2 ずつ）に刻み、刻んだ点ごとに `Collision::SphereSphere` / `SphereAABB` で敵を調べる。精度は刻み幅しだい。時間が無いときはこちら。

### G5. 自由な形のメッシュを描く（`Renderer::DrawMesh`）— 小・**必須**

Game.md 4-D の軌跡の帯（Ribbon Mesh）。今の `Renderer` は決まった形（三角形・四角・球・箱・線）しか出せない。**中ではもう `PushMesh`（頂点と番号の配列を受け取る）が全部の形の共通処理になっている**ので、それを外から呼べるようにするだけ。

```cpp
struct MeshConfig {
	std::vector<Vertex3dData> vertices; // 位置・UV・法線
	std::vector<uint32_t> indices;
	// 以下は他の Config と同じ名前（color・textureHandle・shadingType・blendMode・depthMode・camera など）
};
static void DrawMesh(const MeshConfig& config);
```

- 軌跡の帯はゲームで作る：点ごとに「進む向き × 上（0,1,0）」で左右に押し出し、2頂点ずつ並べる。`Draw()` の中で毎フレーム作り直してよい（頂点はフレームごとの書き込み用バッファに積まれる）。
- **頂点に色が無い**（`Vertex3dData` は位置・UV・法線だけ）。先が細く・薄くなる見た目は、UV とグラデーションのテクスチャで作る。頂点の色が要るなら後で足す。
- 線の帯を四角（`DrawQuad3d`）の並びで描くこともできるが、四角ごとに描画が1回増え、つなぎ目に隙間ができる。

---

## 3. 足す物（あると楽）

### G6. 正射影カメラ — 小

Camera Component に `projection`（Perspective / Orthographic）と `orthoSize`（画面の縦に映る広さ）を足し、`MyEngine_v1/MyEngine/Camera/Camera.cpp:25` で `MakeOrthographicMatrix` を使い分ける（関数はもうある）。G2 のレイは、正射影だと「平行なレイ」になるが、上の作り方（近い点と遠い点を戻す）ならそのまま動く。**低 FOV の透視で足りるなら要らない。**

### G7. 線分と点の距離 — 小（ゲームで書いても可）

インク回収「プレイヤーが線分から r 以内か」。`MathUtility` に `ClosestPointOnSegment(p, a, b)` が無い。数行なのでゲームで書いてもよいが、G4 の `CapsuleSphere` の中でも使うので、G4 と一緒にエンジンに置くと二度書かずに済む。

### G8. 壁の押し戻し（XZ）— 小〜中（ゲームで書いても可）

今の Collider は**重なりを知らせるだけで、止めない**。WASD で壁に入らないようにするには、プレイヤー（球）と壁（箱）の重なりを `Collision::ClosestPointOnAABB` で調べて、外へ押し出す処理を書く。ゲーム側で 30 行ほど。

> Tasks/ActionGame.md の **E2（Body 2D）** は横から見た x・y 用。見下ろし（x・z）にも使える形で作れば共通にできる。

---

## 4. エンジンではなく、ゲームの設計で気をつける所

| 所 | 気をつけること |
|---|---|
| **軌跡の点（`pathPoints`）** | **Component に `std::vector` は入れられない**（Undo・保存がバイト列で写すため。`static_assert` で止まる）。今描いている線の点列は、ゲームに1つの入れ物（`gInk` など）に `std::vector` で持つ。保存しなくてよい（実行中だけの値） |
| 地面に残るインク線 | 「線分1本＝Entity 1つ」にするか、上の入れ物に `std::vector<Segment>` で持つか。数百本になるなら入れ物の方が軽い。回収の判定は G7 の距離で |
| **全体の入れ物は Stop で戻らない** | `gGameState` などはシーンの外にあるので、Play → Stop で中身が残る。シーンの `Initialize`（Play のたびに呼ばれる）で空にする |
| スロー中の入力 | 入力は時間に関係ないので、スロー中もマウスは普通に動く（期待どおり） |
| ドラッグ中に WASD を無効 | `GetMoveAxis()` を読まないだけ（ゲームの状態で分ける） |
| 撃破の瞬間 | `Destroy` の前に、位置を取って爆散インクを置く・`PlayOneShot`・`Time::HitStop`（G1） |

---

## 5. Game.md の予定に合わせた順番

| Game.md の時間 | 先に要るエンジンの物 |
|---|---|
| [01〜04h] 座標変換・WASD・タイムスケール | **G1**・**G2**（＋G6 を使うならここ） |
| [05〜08h] ドラッグ・壁のレイキャスト・ゲージ | **G3** |
| [09〜12h] 高速移動・軌跡メッシュ | **G5** |
| [13〜16h] 斬撃判定・爆散・回収 | **G4**・G7 |
| [17〜20h] ヒットストップ・発光 | G1（ヒットストップの部分）。発光は Bloom のしきい値の調整だけ |

**おすすめ：G1〜G5 を最初にまとめて足す（3〜4 時間）→ あとはゲームだけ。** ActionGame.md と同じ「エンジンの足りない物を先にまとめて足す」の順番。

---

## 6. Tasks/ActionGame.md（2D 横スクロール）との関係

MyEngine_v1 の `Docs/Tasks/ActionGame.md`（未コミット）は、別のゲーム（2D 横スクロール・突進）の案で、E1〜E5 を足す予定になっている。こちらとの重なり：

| ActionGame.md | このゲームで |
|---|---|
| E1 Cube を作るメニュー | 壁を箱で並べるので、あると楽 |
| E2 Body 2D（重力・押し戻し） | 重力は要らない。押し戻しは G8 と共通にできる |
| E3 移動ギズモ・クリックで選ぶ | ステージ（壁・敵）を置くのに、あると楽 |
| E4 空と環境光を暗く | 見下ろしで地面しか映らないなら、無くてもよい |
| E5 UI Image ＋フェード | インクゲージ・タイトル・クリア画面に使える |
| ヒットストップ「そのまま」 | **G1 を直さないと効かない**（両方のゲームに関係する） |

どちらのゲームを作るか（または両方か）で、エンジンに足す順番が変わる。