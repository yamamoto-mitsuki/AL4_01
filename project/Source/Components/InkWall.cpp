#include "InkWall.h"

#include <MyEngine/ECS/GameComponent.h>

COMPONENT(InkWall, "InkSlash") { ui.Label("線を止める壁（Collider の Box を一緒に付ける）"); }