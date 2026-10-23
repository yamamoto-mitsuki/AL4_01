#include "JumpRing.h"

#include "Source/Scripts/IceCube.h"

//=============================================================================
// 使う・戻る
//=============================================================================
void JumpRing::Use() {
	isUsed_ = true;
	OnUse(); // 見た目（派生クラス）
}

void JumpRing::OnTriggerExit(GameObject other) {
	// キューブが輪から出たら、また使える（入り直すと使える）
	if (other.GetComponent<IceCube>() != nullptr) {
		isUsed_ = false;
	}
}