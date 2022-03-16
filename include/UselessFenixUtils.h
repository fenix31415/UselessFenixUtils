#pragma once

namespace FenixUtils
{
	/// <summary>
	/// random less than prop
	/// </summary>
	bool random(float prop);
	void damageav(RE::Actor* victim, RE::ACTOR_VALUE_MODIFIERS::ACTOR_VALUE_MODIFIER i1, RE::ActorValue i2, float val, RE::Actor* attacker);
	RE::TESObjectWEAP* get_UnarmedWeap();
	float PlayerCharacter__get_reach(RE::Actor* a);
}
