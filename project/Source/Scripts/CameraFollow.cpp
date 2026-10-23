#include "CameraFollow.h"

SCRIPT(CameraFollow);

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void CameraFollow::Fields(ComponentUI& ui) {
	ui.Field("Target", target_, Tip("追いかける物（Hierarchy から落とす）"));
	ui.Field("Offset", offset_, Tip("追いかける物からどれだけ離れるか"));
}

//=============================================================================
// エンジンが呼ぶ関数
//=============================================================================
// LateUpdate は全部の Update と SYSTEM の後なので、追いかける物が動き終わった位置へ合わせられる
void CameraFollow::LateUpdate() {
	if (!target_) {
		return; // 入れていない・消えた
	}
	transform.translation = target_.transform.translation + offset_;
}