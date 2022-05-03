#pragma once
#include <boost\type_traits\function_traits.hpp>
#define NOMINMAX
#include <xbyak\xbyak.h>
#include <glm/glm.hpp>

namespace DebugAPI_IMPL
{
	class DebugAPILine
	{
	public:
		DebugAPILine(glm::vec3 from, glm::vec3 to, glm::vec4 color, float lineThickness, unsigned __int64 destroyTickCount);

		glm::vec3 From;
		glm::vec3 To;
		glm::vec4 Color;
		float fColor;
		float Alpha;
		float LineThickness;

		unsigned __int64 DestroyTickCount;
	};

	class DebugAPI
	{
	public:
		static void Update();

		static RE::GPtr<RE::IMenu> GetHUD();

		static void DrawLine2D(RE::GPtr<RE::GFxMovieView> movie, glm::vec2 from, glm::vec2 to, float color, float lineThickness, float alpha);
		static void DrawLine2D(RE::GPtr<RE::GFxMovieView> movie, glm::vec2 from, glm::vec2 to, glm::vec4 color, float lineThickness);
		static void DrawLine3D(RE::GPtr<RE::GFxMovieView> movie, glm::vec3 from, glm::vec3 to, float color, float lineThickness, float alpha);
		static void DrawLine3D(RE::GPtr<RE::GFxMovieView> movie, glm::vec3 from, glm::vec3 to, glm::vec4 color, float lineThickness);
		static void ClearLines2D(RE::GPtr<RE::GFxMovieView> movie);

		static void DrawLineForMS(const glm::vec3& from, const glm::vec3& to, int liftetimeMS = 10, const glm::vec4& color = { 1.0f, 0.0f, 0.0f, 1.0f }, float lineThickness = 1);
		static void DrawSphere(glm::vec3, float radius, int liftetimeMS = 10, const glm::vec4& color = { 1.0f, 0.0f, 0.0f, 1.0f }, float lineThickness = 1);
		static void DrawCircle(glm::vec3, float radius, glm::vec3 eulerAngles, int liftetimeMS = 10, const glm::vec4& color = { 1.0f, 0.0f, 0.0f, 1.0f }, float lineThickness = 1);

		static std::vector<DebugAPILine*> LinesToDraw;

		static bool DEBUG_API_REGISTERED;

		static constexpr int CIRCLE_NUM_SEGMENTS = 32;

		static constexpr float DRAW_LOC_MAX_DIF = 5.0f;

		static glm::vec2 WorldToScreenLoc(RE::GPtr<RE::GFxMovieView> movie, glm::vec3 worldLoc);
		static float RGBToHex(glm::vec3 rgb);

		static void FastClampToScreen(glm::vec2& point);

		// 	static void ClampVectorToScreen(glm::vec2& from, glm::vec2& to);
		// 	static void ClampPointToScreen(glm::vec2& point, float lineAngle);

		static bool IsOnScreen(glm::vec2 from, glm::vec2 to);
		static bool IsOnScreen(glm::vec2 point);

		static void CacheMenuData();

		static bool CachedMenuData;

		static float ScreenResX;
		static float ScreenResY;

	private:
		static float ConvertComponentR(float value);
		static float ConvertComponentG(float value);
		static float ConvertComponentB(float value);
		// returns true if there is already a line with the same color at around the same from and to position
		// with some leniency to bundle together lines in roughly the same spot (see DRAW_LOC_MAX_DIF)
		static DebugAPILine* GetExistingLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color, float lineThickness);
	};

	class DebugOverlayMenu : RE::IMenu
	{
	public:
		static constexpr const char* MENU_PATH = "BetterThirdPersonSelection/overlay_menu";
		static constexpr const char* MENU_NAME = "HUD Menu";

		DebugOverlayMenu();

		static void Register();

		static void Show();
		static void Hide();

		static RE::stl::owner<RE::IMenu*> Creator() { return new DebugOverlayMenu(); }

		void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override;

	private:
		class Logger : public RE::GFxLog
		{
		public:
			void LogMessageVarg(LogMessageType, const char* a_fmt, std::va_list a_argList) override
			{
				std::string fmt(a_fmt ? a_fmt : "");
				while (!fmt.empty() && fmt.back() == '\n') {
					fmt.pop_back();
				}

				std::va_list args;
				va_copy(args, a_argList);
				std::vector<char> buf(static_cast<std::size_t>(std::vsnprintf(0, 0, fmt.c_str(), a_argList) + 1));
				std::vsnprintf(buf.data(), buf.size(), fmt.c_str(), args);
				va_end(args);

				logger::info("{}"sv, buf.data());
			}
		};
	};

	namespace DrawDebug
	{
		namespace Colors
		{
			static constexpr glm::vec4 RED = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			static constexpr glm::vec4 GRN = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			static constexpr glm::vec4 BLU = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
		}

		template <glm::vec4 Color = Colors::RED, int time = 3000>
		void draw_line(const RE::NiPoint3& _from, const RE::NiPoint3& _to, float size = 5.0f)
		{
			glm::vec3 from(_from.x, _from.y, _from.z);
			glm::vec3 to(_to.x, _to.y, _to.z);
			DebugAPI::DrawLineForMS(from, to, time, Color, size);
		}

		template <glm::vec4 Color = Colors::RED>
		void draw_line(const RE::NiPoint3& _from, const RE::NiPoint3& _to, float size = 5.0f, int time = 3000)
		{
			glm::vec3 from(_from.x, _from.y, _from.z);
			glm::vec3 to(_to.x, _to.y, _to.z);
			DebugAPI::DrawLineForMS(from, to, time, Color, size);
		}

		template <glm::vec4 Color = Colors::RED>
		void draw_sphere(const RE::NiPoint3& _center, float r = 5.0f, float size = 5.0f, int time = 3000)
		{
			glm::vec3 center(_center.x, _center.y, _center.z);
			DebugAPI::DrawSphere(center, r, time, Color, size);
		}
	}
}
using namespace DebugAPI_IMPL::DrawDebug;

namespace FenixUtils
{
	/// <summary>
	/// random less than prop
	/// </summary>
	bool random(float prop);
	void damageav(RE::Actor* victim, RE::ACTOR_VALUE_MODIFIERS::ACTOR_VALUE_MODIFIER i1, RE::ActorValue i2, float val, RE::Actor* attacker);
	RE::TESObjectWEAP* get_UnarmedWeap();
	float PlayerCharacter__get_reach(RE::Actor* a);
	float GetHeadingAngle(RE::TESObjectREFR* a, const RE::NiPoint3& a_pos, bool a_abs);
	void UnequipItem(RE::Actor* a, RE::TESBoundObject* item);
	void knock(RE::Actor* target, RE::Actor* aggressor, float KnockDown);
	void cast_spell(RE::Actor* victim, RE::Actor* attacker, RE::SpellItem* spell);
	RE::NiPoint3* Actor__get_eye_pos(RE::Actor* me, RE::NiPoint3* ans, int mb_type);
	float clamp01(float t);

	template <float val = 0.5f>
	void stagger(RE::Actor* victim, RE::Actor* attacker = nullptr)
	{
		float stagDir = 0.0f;
		if (attacker && victim->GetHandle() != attacker->GetHandle()) {
			auto heading = GetHeadingAngle(victim, attacker->GetPosition(), false);
			stagDir = (heading >= 0.0f) ? heading / 360.0f : (360.0f + heading) / 360.0f;
		}

		victim->SetGraphVariableFloat("staggerDirection", stagDir);
		victim->SetGraphVariableFloat("staggerMagnitude", val);
		victim->NotifyAnimationGraph("staggerStart");
	}

	template <int id, typename x_Function>
	class _generic_foo_;

	template <int id, typename T, typename... Args>
	class _generic_foo_<id, T(Args...)>
	{
	public:
		static T eval(Args... args)
		{
			using func_t = T(Args...);
			REL::Relocation<func_t> func{ REL::ID(id) };
			return func(std::forward<Args>(args)...);
		}
	};

	template <size_t BRANCH_TYPE, uint64_t ID, size_t offset = 0, bool call = false>
	void add_trampoline(Xbyak::CodeGenerator* xbyakCode)
	{
		constexpr REL::ID funcOffset = REL::ID(ID);
		auto funcAddr = funcOffset.address();
		auto size = xbyakCode->getSize();
		auto& trampoline = SKSE::GetTrampoline();
		auto result = trampoline.allocate(size);
		std::memcpy(result, xbyakCode->getCode(), size);
		if constexpr (!call)
			trampoline.write_branch<BRANCH_TYPE>(funcAddr + offset, (std::uintptr_t)result);
		else
			trampoline.write_call<BRANCH_TYPE>(funcAddr + offset, (std::uintptr_t)result);
	}

	template<float min, float max>
	static float lerp(float k)
	{
		return min + (max - min) * k;
	}

	void play_sound(RE::TESObjectREFR* a, int formid);
	void play_impact(RE::TESObjectREFR* a, RE::BGSImpactData* impact, RE::NiPoint3* P_V, RE::NiPoint3* P_from, RE::NiNode* bone);
	bool PlayIdle(RE::AIProcess* proc, RE::Actor* attacker, RE::DEFAULT_OBJECT smth, RE::TESIdleForm* idle, bool a5, bool a6, RE::Actor* target);
	float get_total_av(RE::Actor* a, RE::ActorValue av);
	bool TESObjectREFR__HasEffectKeyword(RE::TESObjectREFR* a, RE::BGSKeyword* kwd);

	// ignore cells
	float get_dist2(RE::Actor* a, RE::Actor* b);
}
using FenixUtils::_generic_foo_;
