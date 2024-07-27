#include <UselessFenixUtils.h>

namespace Impl
{
	void PushActorAway_14067D4A0(RE::AIProcess* proc, RE::Actor* target, RE::NiPoint3* AggressorPos, float KnockDown)
	{
		return _generic_foo_<38858, decltype(PushActorAway_14067D4A0)>::eval(proc, target, AggressorPos, KnockDown);
	}

	inline float radToDeg(float a_radians) { return a_radians * (180.0f / RE::NI_PI); }

	bool BGSImpactManager__PlayImpactEffect_impl_1405A2C60(void* manager, RE::TESObjectREFR* a, RE::BGSImpactDataSet* impacts,
		const char* bone_name, RE::NiPoint3* Pick, float length, char abApplyNodeRotation, char abUseNodeLocalRotation)
	{
		return _generic_foo_<35320, decltype(BGSImpactManager__PlayImpactEffect_impl_1405A2C60)>::eval(manager, a, impacts,
			bone_name, Pick, length, abApplyNodeRotation, abUseNodeLocalRotation);
	}

	void* BGSImpactManager__GetSingleton()
	{
		REL::Relocation<RE::NiPointer<void*>*> singleton{ REL::ID(515123) };
		return singleton->get();
	}

	void play_impact(RE::TESObjectCELL* cell, float one, const char* model, RE::NiPoint3* P_V, RE::NiPoint3* P_from, float a6,
		uint32_t _7, RE::NiNode* a8)
	{
		return _generic_foo_<29218, decltype(play_impact)>::eval(cell, one, model, P_V, P_from, a6, _7, a8);
	}

	float Actor__GetActorValueModifier(RE::Actor* a, RE::ACTOR_VALUE_MODIFIER mod, RE::ActorValue av)
	{
		return _generic_foo_<37524, decltype(Actor__GetActorValueModifier)>::eval(a, mod, av);
	}

	uint32_t* placeatme(RE::TESDataHandler* datahandler, uint32_t* handle, RE::TESBoundObject* form, RE::NiPoint3* pos,
		RE::NiPoint3* angle, RE::TESObjectCELL* cell, RE::TESWorldSpace* wrld, RE::TESObjectREFR* a8, char a9, void* a10,
		char persist, char a12)
	{
		return _generic_foo_<13625, decltype(placeatme)>::eval(datahandler, handle, form, pos, angle, cell, wrld, a8, a9, a10,
			persist, a12);
	}

	RE::TESObjectREFR* PlaceAtMe(void* vm, int stack, RE::TESObjectREFR* refr, RE::TESBoundObject* form, uint32_t count,
		char persist, char disabled)
	{
		return _generic_foo_<55672, decltype(PlaceAtMe)>::eval(vm, stack, refr, form, count, persist, disabled);
	}
}
using namespace Impl;

namespace FenixUtils
{
	namespace Geom
	{
		float NiASin(float alpha) { return _generic_foo_<17744, decltype(NiASin)>::eval(alpha); }

		float GetUnclampedZAngleFromVector(const RE::NiPoint3& V)
		{
			return _generic_foo_<68821, decltype(GetUnclampedZAngleFromVector)>::eval(V);
		}

		float GetZAngleFromVector(const RE::NiPoint3& V) { return _generic_foo_<68820, decltype(GetZAngleFromVector)>::eval(V); }

		void CombatUtilities__GetAimAnglesFromVector(const RE::NiPoint3& V, float& rotZ, float& rotX)
		{
			return _generic_foo_<46076, decltype(CombatUtilities__GetAimAnglesFromVector)>::eval(V, rotZ, rotX);
		}

		RE::Projectile::ProjectileRot rot_at(const RE::NiPoint3& V)
		{
			RE::Projectile::ProjectileRot rot;
			CombatUtilities__GetAimAnglesFromVector(V, rot.z, rot.x);
			return rot;
		}

		RE::Projectile::ProjectileRot rot_at(const RE::NiPoint3& from, const RE::NiPoint3& to) { return rot_at(to - from); }

		// Angles -> dir: FromEulerAnglesZYX, A.{A, B, C}.Y.Unitize()

		// Same as FromEulerAnglesZYX.{A, B, C}.Y
		RE::NiPoint3 angles2dir(const RE::NiPoint3& angles)
		{
			RE::NiPoint3 ans;

			float sinx = sinf(angles.x);
			float cosx = cosf(angles.x);
			float sinz = sinf(angles.z);
			float cosz = cosf(angles.z);

			ans.x = cosx * sinz;
			ans.y = cosx * cosz;
			ans.z = -sinx;

			return ans;
		}

		RE::NiPoint3 rotate(float r, const RE::NiPoint3& angles) { return angles2dir(angles) * r; }

		RE::NiPoint3 rotate(const RE::NiPoint3& A, const RE::NiPoint3& angles)
		{
			RE::NiMatrix3 R;
			R.EulerAnglesToAxesZXY(angles);
			return R * A;
		}

		RE::NiPoint3 rotate(const RE::NiPoint3& P, float alpha, const RE::NiPoint3& axis)
		{
			float cos_phi = cos(alpha);
			float sin_phi = sin(alpha);
			float one_cos_phi = 1 - cos_phi;

			RE::NiMatrix3 R = { { cos_phi + one_cos_phi * axis.x * axis.x, axis.x * axis.y * one_cos_phi - axis.z * sin_phi,
									axis.x * axis.z * one_cos_phi + axis.y * sin_phi },
				{ axis.y * axis.x * one_cos_phi + axis.z * sin_phi, cos_phi + axis.y * axis.y * one_cos_phi,
					axis.y * axis.z * one_cos_phi - axis.x * sin_phi },
				{ axis.z * axis.x * one_cos_phi - axis.y * sin_phi, axis.z * axis.y * one_cos_phi + axis.x * sin_phi,
					cos_phi + axis.z * axis.z * one_cos_phi } };

			return R * P;
		}

		RE::NiPoint3 rotate(const RE::NiPoint3& P, float alpha, const RE::NiPoint3& origin, const RE::NiPoint3& axis_dir)
		{
			return rotate(P - origin, alpha, axis_dir) + origin;
		}

		float get_rotation_angle(const RE::NiPoint3& vel_cur, const RE::NiPoint3& dir_final)
		{
			return acosf(std::min(1.0f, vel_cur.Dot(dir_final) / vel_cur.Length()));
		}

		float clamp_angle(float alpha, float max_alpha)
		{
			if (alpha >= 0)
				return std::min(max_alpha, alpha);
			else
				return std::max(-max_alpha, alpha);
		}

		bool is_angle_small(float alpha) { return abs(alpha) < 0.001f; }

		RE::NiPoint3 rotateVel(const RE::NiPoint3& vel_cur, float max_alpha, const RE::NiPoint3& dir_final)
		{
			float needed_angle = get_rotation_angle(vel_cur, dir_final);
			if (!is_angle_small(needed_angle)) {
				float phi = clamp_angle(needed_angle, max_alpha);
				return rotate(vel_cur, phi, vel_cur.UnitCross(dir_final));
			} else {
				return vel_cur;
			}
		}

		namespace Projectile
		{
			void update_node_rotation(RE::Projectile* proj, const RE::NiPoint3& V)
			{
				float rotZ, rotX;
				CombatUtilities__GetAimAnglesFromVector(V, rotZ, rotX);
				TESObjectREFR__SetAngleOnReferenceX(proj, rotX);
				TESObjectREFR__SetAngleOnReferenceZ(proj, rotZ);
				if (auto node = proj->Get3D()) {
					RE::NiMatrix3 M;
					M.EulerAnglesToAxesZXY(rotX, 0, rotZ);
					node->local.rotate = M;
				}
			}

			void update_node_rotation(RE::Projectile* proj) { return update_node_rotation(proj, proj->linearVelocity); }

			void aimToPoint(RE::Projectile* proj, const RE::NiPoint3& P)
			{
				auto dir = P - proj->GetPosition();
				auto angles = FenixUtils::Geom::rot_at(dir);

				FenixUtils::TESObjectREFR__SetAngleOnReferenceX(proj, angles.x);
				FenixUtils::TESObjectREFR__SetAngleOnReferenceZ(proj, angles.z);

				if (proj->IsMissileProjectile()) {
					if (float dist = dir.SqrLength(); dist > 0.0000001f) {
						proj->linearVelocity = dir * sqrtf(proj->linearVelocity.SqrLength() / dist);
					}
					FenixUtils::Geom::Projectile::update_node_rotation(proj);
				}

				proj->flags.reset(RE::Projectile::Flags::kAutoAim);
			}
		}

		namespace Actor
		{
			RE::NiPoint3 CalculateLOSLocation(RE::TESObjectREFR* refr, LineOfSightLocation los_loc)
			{
				return _generic_foo_<46021, decltype(CalculateLOSLocation)>::eval(refr, los_loc);
			}

			RE::NiPoint3 raycast(RE::Actor* caster)
			{
				auto havokWorldScale = RE::bhkWorld::GetWorldScale();
				RE::bhkPickData pick_data;
				RE::NiPoint3 ray_start, ray_end;

				ray_start = CalculateLOSLocation(caster, LineOfSightLocation::kHead);
				ray_end = ray_start + FenixUtils::Geom::rotate(2000000000, caster->data.angle);
				pick_data.rayInput.from = ray_start * havokWorldScale;
				pick_data.rayInput.to = ray_end * havokWorldScale;

				uint32_t collisionFilterInfo = 0;
				caster->GetCollisionFilterInfo(collisionFilterInfo);
				pick_data.rayInput.filterInfo = (static_cast<uint32_t>(collisionFilterInfo >> 16) << 16) |
				                                static_cast<uint32_t>(RE::COL_LAYER::kCharController);

				caster->GetParentCell()->GetbhkWorld()->PickObject(pick_data);
				RE::NiPoint3 hitpos;
				if (pick_data.rayOutput.HasHit()) {
					hitpos = ray_start + (ray_end - ray_start) * pick_data.rayOutput.hitFraction;
				} else {
					hitpos = ray_end;
				}
				return hitpos;
			}

			RE::NiPoint3 AnticipatePos(RE::Actor* a, float dtime)
			{
				RE::NiPoint3 ans, eye_pos;
				a->GetLinearVelocity(ans);
				ans *= dtime;
				eye_pos = CalculateLOSLocation(a, LineOfSightLocation::kTorso);
				ans += eye_pos;
				return ans;
			}

			LineOfSightLocation Actor__CalculateLOS(RE::Actor* caster, RE::Actor* target, float viewCone)
			{
				return _generic_foo_<36752, decltype(Actor__CalculateLOS)>::eval(caster, target, viewCone);
			}

			bool ActorInLOS(RE::Actor* caster, RE::Actor* target, float viewCone)
			{
				return Actor__CalculateLOS(caster, target, viewCone) != LineOfSightLocation::kNone;
			}

			float Actor__CalculateAimDelta(RE::Actor* a, const RE::NiPoint3& target_pos)
			{
				return _generic_foo_<36757, decltype(Actor__CalculateAimDelta)>::eval(a, target_pos);
			}

			bool Actor__IsPointInAimCone(RE::Actor* a, RE::NiPoint3* target_pos, float viewCone)
			{
				return _generic_foo_<36756, decltype(Actor__IsPointInAimCone)>::eval(a, target_pos, viewCone);
			}
		}
	}

	namespace Random
	{
		// random(0..1) <= chance
		bool RandomBoolChance(float prop) { return _generic_foo_<26009, decltype(RandomBoolChance)>::eval(prop); }

		float Float(float min, float max) { return _generic_foo_<14109, decltype(Float)>::eval(min, max); }

		float FloatChecked(float min, float max) { return _generic_foo_<25867, decltype(FloatChecked)>::eval(min, max); }

		float FloatTwoPi() { return _generic_foo_<15093, decltype(FloatTwoPi)>::eval(); }

		float Float0To1() { return _generic_foo_<28658, decltype(FloatTwoPi)>::eval(); }

		float FloatNeg1To1() { return _generic_foo_<28716, decltype(FloatTwoPi)>::eval(); }

		int32_t random_range(int32_t min, int32_t max)
		{
			return _generic_foo_<56478, int32_t(void*, uint32_t, void*, int32_t a_min, int32_t a_max)>::eval(nullptr, 0, nullptr,
				min, max);
		}
	}

	namespace Json
	{
		int get_mod_index(std::string_view name)
		{
			auto esp = RE::TESDataHandler::GetSingleton()->LookupModByName(name);
			if (!esp)
				return -1;
			return !esp->IsLight() ? esp->compileIndex << 24 : (0xFE000 | esp->smallFileCompileIndex) << 12;
		}

		uint32_t get_formid(const std::string& name)
		{
			if (auto pos = name.find('|'); pos != std::string::npos) {
				auto ind = get_mod_index(name.substr(0, pos));
				return ind | std::stoul(name.substr(pos + 1), nullptr, 16);
			} else {
				return std::stoul(name, nullptr, 16);
			}
		}

		RE::NiPoint3 getPoint3(const ::Json::Value& jobj, const std::string& field_name)
		{
			assert(jobj.isMember(field_name));
			auto& point = jobj[field_name];
			return { point[0].asFloat(), point[1].asFloat(), point[2].asFloat() };
		}

		RE::Projectile::ProjectileRot getPoint2(const ::Json::Value& jobj, const std::string& field_name)
		{
			assert(jobj.isMember(field_name));
			auto& point = jobj[field_name];
			return { point[0].asFloat(), point[1].asFloat() };
		}

		RE::Projectile::ProjectileRot mb_getPoint2(const ::Json::Value& jobj, const std::string& field_name)
		{
			return jobj.isMember(field_name) ? getPoint2(jobj, field_name) : RE::Projectile::ProjectileRot{ 0, 0 };
		}

		std::string getString(const ::Json::Value& jobj, const std::string& field_name)
		{
			assert(jobj.isMember(field_name));
			return jobj[field_name].asString();
		}

		std::string mb_getString(const ::Json::Value& jobj, const std::string& field_name)
		{
			return jobj.isMember(field_name) ? getString(jobj, field_name) : "";
		}

		float getFloat(const ::Json::Value& jobj, const std::string& field_name)
		{
			assert(jobj.isMember(field_name));
			return jobj[field_name].asFloat();
		}

		bool getBool(const ::Json::Value& jobj, const std::string& field_name)
		{
			assert(jobj.isMember(field_name));
			return jobj[field_name].asBool();
		}

		uint32_t getUint32(const ::Json::Value& jobj, const std::string& field_name)
		{
			assert(jobj.isMember(field_name));
			return jobj[field_name].asUInt();
		}
	}

	namespace Timer
	{
		float get_time() { return *REL::Relocation<float*>(REL::ID(517597)); }

		bool passed(float timestamp, float interval) { return get_time() - timestamp > interval; }

		bool passed(RE::AITimer& timer) { return passed(timer.aiTimer, timer.timer); }

		void updateAndWait(RE::AITimer& timer, float interval)
		{
			timer.aiTimer = get_time();
			timer.timer = interval;
		}
	}

	namespace Behavior
	{
		RE::hkbNode* lookup_node(RE::hkbBehaviorGraph* root_graph, const char* name)
		{
			RE::BShkbUtils::GraphTraverser traverser(6, root_graph);
			for (auto node = traverser.Next(); node; node = traverser.Next()) {
				if (node->name.c_str() && !std::strcmp(node->name.c_str(), name))
					return node;
			}
			return nullptr;
		}

		int32_t get_implicit_id_variable(RE::hkbBehaviorGraph* graph, const char* name)
		{
			const auto& names = graph->data->stringData->variableNames;
			for (int32_t i = 0; i < names.size(); i++) {
				if (!std::strcmp(names[i].c_str(), name))
					return i;
			}
			return -1;
		}

		int32_t get_implicit_id_event(RE::hkbBehaviorGraph* graph, const char* name)
		{
			const auto& names = graph->data->stringData->eventNames;
			for (int32_t i = 0; i < names.size(); i++) {
				if (!std::strcmp(names[i].c_str(), name))
					return i;
			}
			return RE::hkbEventBase::SystemEventIDs_::kNull;
		}

		const char* get_event_name_implicit(RE::hkbBehaviorGraph* graph, int32_t implicit_id)
		{
			if (implicit_id == RE::hkbEventBase::SystemEventIDs_::kNull)
				return nullptr;

			return graph->data->stringData->eventNames[static_cast<uint32_t>(implicit_id)].c_str();
		}

		const char* get_event_name_explicit(RE::hkbBehaviorGraph* graph, int32_t explicit_id)
		{
			if (auto map = graph->eventIDMap.get()) {
				auto implicit_id = map->map.getWithDefault(static_cast<int64_t>(explicit_id) + 1, -1);
				if (implicit_id != -1) {
					return get_event_name_implicit(graph, static_cast<int32_t>(implicit_id));
				}
			}

			return nullptr;
		}

		const char* get_variable_name(RE::hkbBehaviorGraph* graph, int32_t ind)
		{
			return graph->data->stringData->variableNames[static_cast<uint32_t>(ind)].c_str();
		}
	}

	void tolower(std::string& data)
	{
		std::transform(data.begin(), data.end(), data.begin(), [](unsigned char c) { return std::tolower(c); });
	}

	float Projectile__GetSpeed(RE::Projectile* proj) { return _generic_foo_<42958, decltype(Projectile__GetSpeed)>::eval(proj); }

	void Projectile__set_collision_layer(RE::Projectile* proj, RE::COL_LAYER collayer)
	{
		if (auto shape = (RE::bhkShapePhantom*)proj->unk0E0)
			if (auto ref = (RE::hkpShapePhantom*)shape->referencedObject.get()) {
				auto& colFilterInfo = ref->collidable.broadPhaseHandle.collisionFilterInfo;
				colFilterInfo &= ~0x7F;
				colFilterInfo |= static_cast<uint32_t>(collayer);
			}
	}

	void TESObjectREFR__SetAngleOnReferenceX(RE::TESObjectREFR* refr, float angle_x)
	{
		return _generic_foo_<19360, decltype(TESObjectREFR__SetAngleOnReferenceX)>::eval(refr, angle_x);
	}

	void TESObjectREFR__SetAngleOnReferenceZ(RE::TESObjectREFR* refr, float angle_z)
	{
		return _generic_foo_<19362, decltype(TESObjectREFR__SetAngleOnReferenceZ)>::eval(refr, angle_z);
	}

	RE::TESObjectARMO* GetEquippedShield(RE::Actor* a) { return _generic_foo_<37624, decltype(GetEquippedShield)>::eval(a); }

	RE::EffectSetting* getAVEffectSetting(RE::MagicItem* mgitem)
	{
		return _generic_foo_<11194, decltype(getAVEffectSetting)>::eval(mgitem);
	}

	void damage_stamina_withdelay(RE::Actor* a, float val)
	{
		const RE::ActorValue avStamina = RE::ActorValue::kStamina;

		if (val > 0 && !a->IsInvulnerable()) {
			float old = a->GetActorValue(avStamina);
			a->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, avStamina, -val);
			if (a->GetActorValue(avStamina) <= 0.0f) {
				if (auto proc = a->currentProcess) {
					float rate = _generic_foo_<37515, float(RE::Actor*, RE::ActorValue)>::eval(a, avStamina);
					if (rate > 0.0001f) {
						float delay = (val - old) / rate;
						_generic_foo_<38526, void(RE::AIProcess*, RE::ActorValue, float)>::eval(proc, avStamina, delay);
						if (a->IsPlayerRef())
							_generic_foo_<51907, void(RE::ActorValue)>::eval(avStamina);
					}
				}
			}
		}
	}

	void damageav_attacker(RE::Actor* victim, RE::ACTOR_VALUE_MODIFIERS::ACTOR_VALUE_MODIFIER i1, RE::ActorValue i2, float val,
		RE::Actor* attacker)
	{
		return _generic_foo_<37523, decltype(damageav_attacker)>::eval(victim, i1, i2, val, attacker);
	}

	void damageav(RE::Actor* a, RE::ActorValue av, float val)
	{
		a->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, av, -val);
	}

	RE::TESObjectWEAP* get_UnarmedWeap()
	{
		constexpr REL::ID UnarmedWeap(static_cast<std::uint64_t>(514923));
		REL::Relocation<RE::NiPointer<RE::TESObjectWEAP>*> singleton{ UnarmedWeap };
		return singleton->get();
	}

	bool PlayIdle(RE::AIProcess* proc, RE::Actor* attacker, RE::DEFAULT_OBJECT smth, RE::TESIdleForm* idle, bool a5, bool a6,
		RE::Actor* target)
	{
		return _generic_foo_<38290, decltype(PlayIdle)>::eval(proc, attacker, smth, idle, a5, a6, target);
	}

	float PlayerCharacter__get_reach(RE::Actor* a) { return _generic_foo_<37588, decltype(PlayerCharacter__get_reach)>::eval(a); }

	float GetHeadingAngle(RE::TESObjectREFR* a, const RE::NiPoint3& a_pos, bool a_abs)
	{
		float theta = RE::NiFastATan2(a_pos.x - a->GetPositionX(), a_pos.y - a->GetPositionY());
		float heading = Impl::radToDeg(theta - a->GetAngleZ());

		if (heading < -180.0f) {
			heading += 360.0f;
		}

		if (heading > 180.0f) {
			heading -= 360.0f;
		}

		return a_abs ? RE::NiAbs(heading) : heading;
	}

	void UnequipItem(RE::Actor* a, RE::TESBoundObject* item) { RE::ActorEquipManager::GetSingleton()->UnequipObject(a, item); }

	void knock(RE::Actor* target, RE::Actor* aggressor, float KnockDown)
	{
		if (target->currentProcess)
			Impl::PushActorAway_14067D4A0(target->currentProcess, target, &aggressor->data.location, KnockDown);
	}

	void cast_spell(RE::Actor* victim, RE::Actor* attacker, RE::SpellItem* spell)
	{
		RE::MagicCaster* caster = attacker->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		if (caster && spell) {
			caster->CastSpellImmediate(spell, false, victim, 1.0f, false, 0.0f, attacker);
		}
	}

	void play_sound(RE::TESObjectREFR* a, int formid)
	{
		RE::BSSoundHandle handle;
		handle.soundID = static_cast<uint32_t>(-1);
		handle.assumeSuccess = false;
		*(uint32_t*)&handle.state = 0;

		auto manager = _generic_foo_<66391, void*()>::eval();
		_generic_foo_<66401, int(void*, RE::BSSoundHandle*, int, int)>::eval(manager, &handle, formid, 16);
		if (_generic_foo_<66370, bool(RE::BSSoundHandle*, float, float, float)>::eval(&handle, a->data.location.x,
				a->data.location.y, a->data.location.z)) {
			_generic_foo_<66375, void(RE::BSSoundHandle*, RE::NiAVObject*)>::eval(&handle, a->Get3D());
			_generic_foo_<66355, bool(RE::BSSoundHandle*)>::eval(&handle);
		}
	}

	bool play_impact_(RE::TESObjectREFR* a, int)
	{
		auto impacts = RE::TESForm::LookupByID<RE::BGSImpactDataSet>(0x000183FF);
		//auto impacts = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSImpactDataSet>(RE::FormID(0x1A3FB), "Dawnguard.esm");
		RE::NiPoint3 Pick = { 0.0f, 0.0f, 1.0f };
		return BGSImpactManager__PlayImpactEffect_impl_1405A2C60(BGSImpactManager__GetSingleton(), a, impacts, "WEAPON", &Pick,
			125.0f, true, false);
	}

	void play_impact(RE::TESObjectREFR* a, RE::BGSImpactData* impact, RE::NiPoint3* P_V, RE::NiPoint3* P_from, RE::NiNode* bone)
	{
		Impl::play_impact(a->GetParentCell(), 1.0f, impact->GetModel(), P_V, P_from, 1.0f, 7, bone);
	}

	float clamp01(float t) { return std::max(0.0f, std::min(1.0f, t)); }

	float get_total_av(RE::Actor* a, RE::ActorValue av)
	{
		float permanent = a->GetPermanentActorValue(av);
		float temporary = Actor__GetActorValueModifier(a, RE::ACTOR_VALUE_MODIFIER::kTemporary, av);
		return permanent + temporary;
	}

	float get_dist2(RE::TESObjectREFR* a, RE::TESObjectREFR* b) { return a->GetPosition().GetSquaredDistance(b->GetPosition()); }

	bool TESObjectREFR__HasEffectKeyword(RE::TESObjectREFR* a, RE::BGSKeyword* kwd)
	{
		return _generic_foo_<19220, decltype(TESObjectREFR__HasEffectKeyword)>::eval(a, kwd);
	}

	RE::BGSAttackDataPtr get_attackData(RE::Actor* a)
	{
		if (auto proc = a->currentProcess) {
			if (auto high = proc->high) {
				return high->attackData;
			}
		}
		return nullptr;
	}

	bool is_powerattacking(RE::Actor* a) { return _generic_foo_<37639, decltype(is_powerattacking)>::eval(a); }

	RE::TESObjectWEAP* get_weapon(RE::Actor* a)
	{
		if (auto __weap = a->GetAttackingWeapon()) {
			if (auto _weap = __weap->object; _weap->IsWeapon()) {
				return _weap->As<RE::TESObjectWEAP>();
			}
		}

		return nullptr;
	}

	bool is_human(RE::Actor* a)
	{
		auto race = a->GetRace();
		if (!race)
			return false;

		auto flag = race->validEquipTypes.underlying();
		auto mask = static_cast<uint32_t>(RE::TESRace::EquipmentFlag::kOneHandSword) |
		            static_cast<uint32_t>(RE::TESRace::EquipmentFlag::kTwoHandSword);
		return (flag & mask) > 0;
	}

	void set_RegenDelay(RE::AIProcess* proc, RE::ActorValue av, float time)
	{
		return _generic_foo_<38526, decltype(set_RegenDelay)>::eval(proc, av, time);
	}

	void FlashHudMenuMeter__blink(RE::ActorValue av) { _generic_foo_<51907, decltype(FlashHudMenuMeter__blink)>::eval(av); }

	float get_regen(RE::Actor* a, RE::ActorValue av) { return _generic_foo_<37515, decltype(get_regen)>::eval(a, av); }

	void damagestamina_delay_blink(RE::Actor* a, float cost)
	{
		float old_stamina = a->GetActorValue(RE::ActorValue::kStamina);
		damageav(a, RE::ActorValue::kStamina, cost);
		if (a->GetActorValue(RE::ActorValue::kStamina) <= 0.0f) {
			if (auto proc = a->currentProcess) {
				float StaminaRegenDelay = (cost - old_stamina) / get_regen(a, RE::ActorValue::kStamina);
				set_RegenDelay(proc, RE::ActorValue::kStamina, StaminaRegenDelay);
				if (a->IsPlayerRef()) {
					FlashHudMenuMeter__blink(RE::ActorValue::kStamina);
				}
			}
		}
	}

	float getAV_pers(RE::Actor* a, RE::ActorValue av)
	{
		float all = get_total_av(a, av);
		if (all < 0.000001)
			return 1.0f;

		float cur = a->GetActorValue(av);
		if (cur < 0.0f)
			return 0.0f;

		return cur / all;
	}

	float lerp(float x, float Ax, float Ay, float Bx, float By)
	{
		if (abs(Bx - Ax) < 0.0000001)
			return Ay;

		return (Ay * (Bx - x) + By * (x - Ax)) / (Bx - Ax);
	}

	void AddItem(RE::Actor* a, RE::TESBoundObject* item, RE::ExtraDataList* extraList, int count, RE::TESObjectREFR* fromRefr)
	{
		return _generic_foo_<36525, decltype(AddItem)>::eval(a, item, extraList, count, fromRefr);
	}

	void AddItemPlayer(RE::TESBoundObject* item, int count)
	{
		return AddItem(RE::PlayerCharacter::GetSingleton(), item, nullptr, count, nullptr);
	}

	int RemoveItemPlayer(RE::TESBoundObject* item, int count)
	{
		return _generic_foo_<16564, decltype(RemoveItemPlayer)>::eval(item, count);
	}

	int get_item_count(RE::Actor* a, RE::TESBoundObject* item)
	{
		if (auto changes = a->GetInventoryChanges()) {
			return _generic_foo_<15868, int(RE::InventoryChanges*, RE::TESBoundObject*)>::eval(changes, item);
		}

		return 0;
	}

	uint32_t* placeatme(RE::TESObjectREFR* a, uint32_t* handle, RE::TESBoundObject* form, RE::NiPoint3* pos, RE::NiPoint3* angle)
	{
		return Impl::placeatme(RE::TESDataHandler::GetSingleton(), handle, form, pos, angle, a->GetParentCell(),
			a->GetWorldspace(), nullptr, 0, nullptr, false, 1);
	}

	RE::TESObjectREFR* placeatmepap(RE::TESObjectREFR* a, RE::TESBoundObject* form, int count)
	{
		if (!form || !form->IsBoundObject())
			return nullptr;

		return Impl::PlaceAtMe(nullptr, 0, a, form, count, false, false);
	}

	bool is_playable_spel(RE::SpellItem* spel)
	{
		using ST = RE::MagicSystem::SpellType;
		using AV = RE::ActorValue;

		auto type = spel->GetSpellType();
		if (type == ST::kStaffEnchantment || type == ST::kScroll || type == ST::kSpell || type == ST::kLeveledSpell) {
			auto av = spel->GetAssociatedSkill();
			return av == AV::kAlteration || av == AV::kConjuration || av == AV::kDestruction || av == AV::kIllusion ||
			       av == AV::kRestoration;
		}

		return false;
	}
}
