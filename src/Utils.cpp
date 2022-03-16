
namespace FenixUtils
{
	bool random(float prop)
	{
		if (prop <= 0.0f)
			return false;
		if (prop >= 1.0f)
			return true;

		float random = ((float)rand()) / (float)RAND_MAX;
		return random <= prop;
	}

	void damageav(RE::Actor* victim, RE::ACTOR_VALUE_MODIFIERS::ACTOR_VALUE_MODIFIER i1, RE::ActorValue i2, float val, RE::Actor* attacker)
	{
		using func_t = decltype(&damageav);
		REL::Relocation<func_t> func{ REL::ID(37523) };
		return func(victim, i1, i2, val, attacker);
	}

	RE::TESObjectWEAP* get_UnarmedWeap()
	{
		constexpr REL::ID UnarmedWeap(static_cast<std::uint64_t>(514923));
		REL::Relocation<RE::NiPointer<RE::TESObjectWEAP>*> singleton{ UnarmedWeap };
		return singleton->get();
	}

	bool PlayIdle(RE::AIProcess* proc, RE::Actor* attacker, RE::DEFAULT_OBJECT smth, RE::TESIdleForm* idle, bool a5, bool a6, RE::Actor* target)
	{
		using func_t = decltype(&PlayIdle);
		REL::Relocation<func_t> func{ REL::ID(38290) };
		return func(proc, attacker, smth, idle, a5, a6, target);
	}

	float PlayerCharacter__get_reach(RE::Actor* a)
	{
		using func_t = decltype(&PlayerCharacter__get_reach);
		REL::Relocation<func_t> func{ REL::ID(37588) };
		return func(a);
	}
}
