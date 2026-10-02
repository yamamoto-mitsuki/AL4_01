#include "InkEnemy.h"

#include <MyEngine/ECS/GameComponent.h>

COMPONENT(InkEnemy, "InkSlash") { ui.Field("HP", value.hp, AtLeast(1.0f).Tip("斬られるたびに1減り、0で消える")); }