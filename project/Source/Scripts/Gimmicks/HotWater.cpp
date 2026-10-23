#include "HotWater.h"

#include "Source/Scripts/IceCube.h"

SCRIPT(HotWater, "Gimmick");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void HotWater::Fields(ComponentUI& ui) { ui.Label("値は無い。キューブが触れたら一発アウト（溶ける）"); }

//=============================================================================
// 当たり
//=============================================================================
void HotWater::OnTriggerEnter(GameObject other) {
	// 何回呼んでも、やられるのは1回だけ（IceCube::Kill が、もうやられていたら何もしない）
	if (IceCube* cube = other.GetComponent<IceCube>()) {
		cube->Kill(IceCube::DeathType::Melt);
	}
}

void HotWater::OnTriggerStay(GameObject other) { OnTriggerEnter(other); }