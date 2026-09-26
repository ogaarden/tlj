#ifndef ICONS_HPP
#define ICONS_HPP

#include <raylib.h>
#include "ability_types.hpp"
#include "shop.hpp"

// =====================================================================
// IKONER tegnet med former (skalerer skarpt, trenger ingen bildefiler).
// `size` er omtrent radiusen til ikonet. Brukes i shoppen, level-up og HUD.
// =====================================================================

// Ikon for en ability. AbilityId::COUNT gir et helbredelses-hjerte (brukes av "Restituer").
void DrawAbilityIcon(AbilityId id, Vector2 center, float size);

// Ikon for en permanent oppgradering i shoppen
void DrawUpgradeIcon(ShopUpgrade upgrade, Vector2 center, float size);

#endif // ICONS_HPP
