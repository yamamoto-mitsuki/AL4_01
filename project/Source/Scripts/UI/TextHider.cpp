#include "TextHider.h"

#include <MyEngine/Components/TextComponent.h>
#include <MyEngine/ECS/EntityManager.h>
#include <MyEngine/UI/UIScreen.h>

//=============================================================================
// 覚える・濃さを変える
//=============================================================================
void TextHider::Capture() {
	texts_.clear();
	EntityManager::ForEach<TextComponent>([this](Handle<Entity> entity, const TextComponent& text) {
		if (!UIScreen::IsInsideOpenScreen(entity)) {
			texts_.push_back({.entity = entity, .alpha = text.color.w});
		}
	});
}

void TextHider::SetAlpha(float rate) const {
	for (const HiddenText& hidden : texts_) {
		if (TextComponent* text = EntityManager::Get<TextComponent>(hidden.entity)) {
			text->color.w = hidden.alpha * rate;
		}
	}
}
