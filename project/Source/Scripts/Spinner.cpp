#include "Spinner.h"

SCRIPT(Spinner);

void Spinner::Update() { Rotate(transform, speed_ * Time::GetDeltaTime()); }

void Spinner::Fields(ComponentUI& ui) { ui.Field("Speed", speed_, Tip("1秒に回す角度（度）")); }