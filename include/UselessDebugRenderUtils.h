#pragma once

namespace DebugRenderUtils
{
	class UpdateHooks
	{
	public:
		static void Hook() { _Nullsub = SKSE::GetTrampoline().write_call<5>(REL::ID(35565).address() + 0x748, Nullsub); }

	private:
		static void Nullsub();
		static inline REL::Relocation<decltype(Nullsub)> _Nullsub;
	};

	void OnMessage(SKSE::MessagingInterface::Message* message);

	namespace DrawDebug
	{
		namespace Colors
		{
			static constexpr glm::vec4 RED = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			static constexpr glm::vec4 GRN = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			static constexpr glm::vec4 BLU = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
		}

		void draw_line(const RE::NiPoint3& start, const RE::NiPoint3& end, glm::vec4 color = Colors::RED, float duration = 3.0f,
			bool drawOnTop = false);
		void draw_line0(const RE::NiPoint3& start, const RE::NiPoint3& end, glm::vec4 color = Colors::RED,
			bool drawOnTop = false);
		void draw_sphere(const RE::NiPoint3& center, float radius, glm::vec4 color = Colors::RED, float duration = 3.0f,
			bool drawOnTop = false);
		void draw_sphere0(const RE::NiPoint3& center, float radius, glm::vec4 color = Colors::RED, bool drawOnTop = false);
		void draw_point(const RE::NiPoint3& position, glm::vec4 color = Colors::RED, float duration = 3.0f,
			bool drawOnTop = false);
		void draw_point0(const RE::NiPoint3& position, glm::vec4 color = Colors::RED, bool drawOnTop = false);

		void draw_shape(const RE::CombatMathUtilities::Capsule& cap, glm::vec4 color = Colors::RED, float duration = 3.0f,
			bool drawOnTop = false);
		void draw_shape0(const RE::CombatMathUtilities::Capsule& cap, glm::vec4 color, bool drawOnTop = false);

		void draw_shape(const RE::CombatMathUtilities::Sphere& s, glm::vec4 color = Colors::RED, float duration = 3.0f,
			bool drawOnTop = false);
		void draw_shape0(const RE::CombatMathUtilities::Sphere& s, glm::vec4 color = Colors::RED, bool drawOnTop = false);

		void draw_shape(RE::CombatMathUtilities::MovingCapsule cap, glm::vec4 color = Colors::RED, float duration = 3.0f,
			bool drawOnTop = false);
		void draw_shape0(RE::CombatMathUtilities::MovingCapsule cap, glm::vec4 color = Colors::RED, bool drawOnTop = false);

		void draw_shape(RE::CombatMathUtilities::MovingSphere s, glm::vec4 color = Colors::RED, float duration = 3.0f,
			bool drawOnTop = false);
		void draw_shape0(RE::CombatMathUtilities::MovingSphere s, glm::vec4 color = Colors::RED, bool drawOnTop = false);
	}
}
using namespace DebugRenderUtils::DrawDebug;
