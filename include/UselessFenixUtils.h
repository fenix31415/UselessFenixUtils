#pragma once
#include <boost\type_traits\function_traits.hpp>
#define NOGDI
#pragma warning(push)
#pragma warning(disable: 4702)
#include <xbyak\xbyak.h>
#pragma warning(pop)

#include <glm/glm.hpp>

#include "json/json.h"
#include "magic_enum.hpp"

namespace DebugRender
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
using namespace DebugRender::DrawDebug;

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <dxgi.h>

namespace ImguiUtils
{
	template <bool (*process)(RE::InputEvent* const* evns), void (*show)(), bool (*skipevents)()>
	class ImGuiHooks
	{
	public:
		static void Initialize()
		{
			init_impl();
			Hook();
		}

	private:
		static void Hook()
		{
			auto& trampoline = SKSE::GetTrampoline();

			_DispatchInputEvent = trampoline.write_call<5>(REL::ID(67315).address() + 0x7B, DispatchInputEvent);
			_Present = trampoline.write_call<5>(REL::ID(75461).address() + 0x9, Present);
		}

		static void init_impl()
		{
			const auto renderManager = RE::BSGraphics::Renderer::GetSingleton();

			const auto device = renderManager->data.forwarder;
			const auto context = renderManager->data.context;
			const auto swapChain = renderManager->data.renderWindows[0].swapChain;

			DXGI_SWAP_CHAIN_DESC sd{};
			swapChain->GetDesc(&sd);

			ImGui::CreateContext();

			auto& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
			io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
			io.ConfigWindowsMoveFromTitleBarOnly = true;
			//io.MouseDrawCursor = true;
			io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\arial.ttf", 14, nullptr, io.Fonts->GetGlyphRangesCyrillic());

			ImGui_ImplWin32_Init(sd.OutputWindow);
			ImGui_ImplDX11_Init(device, context);

			ImGui::StyleColorsDark();

			WndProcHook__func = reinterpret_cast<WNDPROC>(
				SetWindowLongPtrA(sd.OutputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook__thunk)));
		}

		static void new_frame()
		{
			ImGui_ImplWin32_NewFrame();
			ImGui_ImplDX11_NewFrame();
			{
				// trick imgui into rendering at game's real resolution (ie. if upscaled with Display Tweaks)
				static const auto screenSize = RE::BSGraphics::Renderer::GetScreenSize();

				auto& io = ImGui::GetIO();
				io.DisplaySize.x = static_cast<float>(screenSize.width);
				io.DisplaySize.y = static_cast<float>(screenSize.height);
			}
			ImGui::NewFrame();

			show();

			ImGui::EndFrame();
			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}

#define IM_VK_KEYPAD_ENTER (VK_RETURN + 256)
		static ImGuiKey ImGui_ImplWin32_VirtualKeyToImGuiKey(WPARAM wParam)
		{
			switch (wParam) {
			case VK_TAB:
				return ImGuiKey_Tab;
			case VK_LEFT:
				return ImGuiKey_LeftArrow;
			case VK_RIGHT:
				return ImGuiKey_RightArrow;
			case VK_UP:
				return ImGuiKey_UpArrow;
			case VK_DOWN:
				return ImGuiKey_DownArrow;
			case VK_PRIOR:
				return ImGuiKey_PageUp;
			case VK_NEXT:
				return ImGuiKey_PageDown;
			case VK_HOME:
				return ImGuiKey_Home;
			case VK_END:
				return ImGuiKey_End;
			case VK_INSERT:
				return ImGuiKey_Insert;
			case VK_DELETE:
				return ImGuiKey_Delete;
			case VK_BACK:
				return ImGuiKey_Backspace;
			case VK_SPACE:
				return ImGuiKey_Space;
			case VK_RETURN:
				return ImGuiKey_Enter;
			case VK_ESCAPE:
				return ImGuiKey_Escape;
			case VK_OEM_7:
				return ImGuiKey_Apostrophe;
			case VK_OEM_COMMA:
				return ImGuiKey_Comma;
			case VK_OEM_MINUS:
				return ImGuiKey_Minus;
			case VK_OEM_PERIOD:
				return ImGuiKey_Period;
			case VK_OEM_2:
				return ImGuiKey_Slash;
			case VK_OEM_1:
				return ImGuiKey_Semicolon;
			case VK_OEM_PLUS:
				return ImGuiKey_Equal;
			case VK_OEM_4:
				return ImGuiKey_LeftBracket;
			case VK_OEM_5:
				return ImGuiKey_Backslash;
			case VK_OEM_6:
				return ImGuiKey_RightBracket;
			case VK_OEM_3:
				return ImGuiKey_GraveAccent;
			case VK_CAPITAL:
				return ImGuiKey_CapsLock;
			case VK_SCROLL:
				return ImGuiKey_ScrollLock;
			case VK_NUMLOCK:
				return ImGuiKey_NumLock;
			case VK_SNAPSHOT:
				return ImGuiKey_PrintScreen;
			case VK_PAUSE:
				return ImGuiKey_Pause;
			case VK_NUMPAD0:
				return ImGuiKey_Keypad0;
			case VK_NUMPAD1:
				return ImGuiKey_Keypad1;
			case VK_NUMPAD2:
				return ImGuiKey_Keypad2;
			case VK_NUMPAD3:
				return ImGuiKey_Keypad3;
			case VK_NUMPAD4:
				return ImGuiKey_Keypad4;
			case VK_NUMPAD5:
				return ImGuiKey_Keypad5;
			case VK_NUMPAD6:
				return ImGuiKey_Keypad6;
			case VK_NUMPAD7:
				return ImGuiKey_Keypad7;
			case VK_NUMPAD8:
				return ImGuiKey_Keypad8;
			case VK_NUMPAD9:
				return ImGuiKey_Keypad9;
			case VK_DECIMAL:
				return ImGuiKey_KeypadDecimal;
			case VK_DIVIDE:
				return ImGuiKey_KeypadDivide;
			case VK_MULTIPLY:
				return ImGuiKey_KeypadMultiply;
			case VK_SUBTRACT:
				return ImGuiKey_KeypadSubtract;
			case VK_ADD:
				return ImGuiKey_KeypadAdd;
			case IM_VK_KEYPAD_ENTER:
				return ImGuiKey_KeypadEnter;
			case VK_LSHIFT:
				return ImGuiKey_LeftShift;
			case VK_LCONTROL:
				return ImGuiKey_LeftCtrl;
			case VK_LMENU:
				return ImGuiKey_LeftAlt;
			case VK_LWIN:
				return ImGuiKey_LeftSuper;
			case VK_RSHIFT:
				return ImGuiKey_RightShift;
			case VK_RCONTROL:
				return ImGuiKey_RightCtrl;
			case VK_RMENU:
				return ImGuiKey_RightAlt;
			case VK_RWIN:
				return ImGuiKey_RightSuper;
			case VK_APPS:
				return ImGuiKey_Menu;
			case '0':
				return ImGuiKey_0;
			case '1':
				return ImGuiKey_1;
			case '2':
				return ImGuiKey_2;
			case '3':
				return ImGuiKey_3;
			case '4':
				return ImGuiKey_4;
			case '5':
				return ImGuiKey_5;
			case '6':
				return ImGuiKey_6;
			case '7':
				return ImGuiKey_7;
			case '8':
				return ImGuiKey_8;
			case '9':
				return ImGuiKey_9;
			case 'A':
				return ImGuiKey_A;
			case 'B':
				return ImGuiKey_B;
			case 'C':
				return ImGuiKey_C;
			case 'D':
				return ImGuiKey_D;
			case 'E':
				return ImGuiKey_E;
			case 'F':
				return ImGuiKey_F;
			case 'G':
				return ImGuiKey_G;
			case 'H':
				return ImGuiKey_H;
			case 'I':
				return ImGuiKey_I;
			case 'J':
				return ImGuiKey_J;
			case 'K':
				return ImGuiKey_K;
			case 'L':
				return ImGuiKey_L;
			case 'M':
				return ImGuiKey_M;
			case 'N':
				return ImGuiKey_N;
			case 'O':
				return ImGuiKey_O;
			case 'P':
				return ImGuiKey_P;
			case 'Q':
				return ImGuiKey_Q;
			case 'R':
				return ImGuiKey_R;
			case 'S':
				return ImGuiKey_S;
			case 'T':
				return ImGuiKey_T;
			case 'U':
				return ImGuiKey_U;
			case 'V':
				return ImGuiKey_V;
			case 'W':
				return ImGuiKey_W;
			case 'X':
				return ImGuiKey_X;
			case 'Y':
				return ImGuiKey_Y;
			case 'Z':
				return ImGuiKey_Z;
			case VK_F1:
				return ImGuiKey_F1;
			case VK_F2:
				return ImGuiKey_F2;
			case VK_F3:
				return ImGuiKey_F3;
			case VK_F4:
				return ImGuiKey_F4;
			case VK_F5:
				return ImGuiKey_F5;
			case VK_F6:
				return ImGuiKey_F6;
			case VK_F7:
				return ImGuiKey_F7;
			case VK_F8:
				return ImGuiKey_F8;
			case VK_F9:
				return ImGuiKey_F9;
			case VK_F10:
				return ImGuiKey_F10;
			case VK_F11:
				return ImGuiKey_F11;
			case VK_F12:
				return ImGuiKey_F12;
			default:
				return ImGuiKey_None;
			}
		}

		class CharEvent : public RE::InputEvent
		{
		public:
			uint32_t keyCode;  // 18 (ascii code)
		};

		static ImGuiKey ParseKeyFromKeyboard(RE::BSKeyboardDevice::Key a_key)
		{
			switch (a_key) {
			// 512
			case RE::BSKeyboardDevice::Key::kTab:
				return ImGuiKey_Tab;
			case RE::BSKeyboardDevice::Key::kLeft:
				return ImGuiKey_LeftArrow;
			case RE::BSKeyboardDevice::Key::kRight:
				return ImGuiKey_RightArrow;
			case RE::BSKeyboardDevice::Key::kUp:
				return ImGuiKey_UpArrow;
			case RE::BSKeyboardDevice::Key::kDown:
				return ImGuiKey_DownArrow;
			case RE::BSKeyboardDevice::Key::kPageUp:
				return ImGuiKey_PageUp;
			case RE::BSKeyboardDevice::Key::kPageDown:
				return ImGuiKey_PageDown;
			case RE::BSKeyboardDevice::Key::kHome:
				return ImGuiKey_Home;
			case RE::BSKeyboardDevice::Key::kEnd:
				return ImGuiKey_End;
			case RE::BSKeyboardDevice::Key::kInsert:
				return ImGuiKey_Insert;
			case RE::BSKeyboardDevice::Key::kDelete:
				return ImGuiKey_Delete;
			case RE::BSKeyboardDevice::Key::kBackspace:
				return ImGuiKey_Backspace;
			case RE::BSKeyboardDevice::Key::kSpacebar:
				return ImGuiKey_Space;
			case RE::BSKeyboardDevice::Key::kEnter:
				return ImGuiKey_Enter;
			case RE::BSKeyboardDevice::Key::kEscape:
				return ImGuiKey_Escape;
			case RE::BSKeyboardDevice::Key::kLeftControl:
				return ImGuiKey_ModCtrl;
			case RE::BSKeyboardDevice::Key::kLeftShift:
				return ImGuiKey_ModShift;
			case RE::BSKeyboardDevice::Key::kLeftAlt:
				return ImGuiKey_ModAlt;
			case RE::BSKeyboardDevice::Key::kLeftWin:
				return ImGuiKey_ModSuper;
			case RE::BSKeyboardDevice::Key::kRightControl:
				return ImGuiKey_ModCtrl;
			case RE::BSKeyboardDevice::Key::kRightShift:
				return ImGuiKey_ModShift;
			case RE::BSKeyboardDevice::Key::kRightAlt:
				return ImGuiKey_ModAlt;
			case RE::BSKeyboardDevice::Key::kRightWin:
				return ImGuiKey_ModSuper;
			// Skip 535
			case RE::BSKeyboardDevice::Key::kNum0:
				return ImGuiKey_0;
			case RE::BSKeyboardDevice::Key::kNum1:
				return ImGuiKey_1;
			case RE::BSKeyboardDevice::Key::kNum2:
				return ImGuiKey_2;
			case RE::BSKeyboardDevice::Key::kNum3:
				return ImGuiKey_3;
			case RE::BSKeyboardDevice::Key::kNum4:
				return ImGuiKey_4;
			case RE::BSKeyboardDevice::Key::kNum5:
				return ImGuiKey_5;
			case RE::BSKeyboardDevice::Key::kNum6:
				return ImGuiKey_6;
			case RE::BSKeyboardDevice::Key::kNum7:
				return ImGuiKey_7;
			case RE::BSKeyboardDevice::Key::kNum8:
				return ImGuiKey_8;
			case RE::BSKeyboardDevice::Key::kNum9:
				return ImGuiKey_9;
			case RE::BSKeyboardDevice::Key::kA:
				return ImGuiKey_A;
			case RE::BSKeyboardDevice::Key::kB:
				return ImGuiKey_B;
			case RE::BSKeyboardDevice::Key::kC:
				return ImGuiKey_C;
			case RE::BSKeyboardDevice::Key::kD:
				return ImGuiKey_D;
			case RE::BSKeyboardDevice::Key::kE:
				return ImGuiKey_E;
			case RE::BSKeyboardDevice::Key::kF:
				return ImGuiKey_F;
			case RE::BSKeyboardDevice::Key::kG:
				return ImGuiKey_G;
			case RE::BSKeyboardDevice::Key::kH:
				return ImGuiKey_H;
			case RE::BSKeyboardDevice::Key::kI:
				return ImGuiKey_I;
			case RE::BSKeyboardDevice::Key::kJ:
				return ImGuiKey_J;
			case RE::BSKeyboardDevice::Key::kK:
				return ImGuiKey_K;
			case RE::BSKeyboardDevice::Key::kL:
				return ImGuiKey_L;
			case RE::BSKeyboardDevice::Key::kM:
				return ImGuiKey_M;
			case RE::BSKeyboardDevice::Key::kN:
				return ImGuiKey_N;
			case RE::BSKeyboardDevice::Key::kO:
				return ImGuiKey_O;
			case RE::BSKeyboardDevice::Key::kP:
				return ImGuiKey_P;
			case RE::BSKeyboardDevice::Key::kQ:
				return ImGuiKey_Q;
			case RE::BSKeyboardDevice::Key::kR:
				return ImGuiKey_R;
			case RE::BSKeyboardDevice::Key::kS:
				return ImGuiKey_S;
			case RE::BSKeyboardDevice::Key::kT:
				return ImGuiKey_T;
			case RE::BSKeyboardDevice::Key::kU:
				return ImGuiKey_U;
			case RE::BSKeyboardDevice::Key::kV:
				return ImGuiKey_V;
			case RE::BSKeyboardDevice::Key::kW:
				return ImGuiKey_W;
			case RE::BSKeyboardDevice::Key::kX:
				return ImGuiKey_X;
			case RE::BSKeyboardDevice::Key::kY:
				return ImGuiKey_Y;
			case RE::BSKeyboardDevice::Key::kZ:
				return ImGuiKey_Z;
			case RE::BSKeyboardDevice::Key::kF1:
				return ImGuiKey_F1;
			case RE::BSKeyboardDevice::Key::kF2:
				return ImGuiKey_F2;
			case RE::BSKeyboardDevice::Key::kF3:
				return ImGuiKey_F3;
			case RE::BSKeyboardDevice::Key::kF4:
				return ImGuiKey_F4;
			case RE::BSKeyboardDevice::Key::kF5:
				return ImGuiKey_F5;
			case RE::BSKeyboardDevice::Key::kF6:
				return ImGuiKey_F6;
			case RE::BSKeyboardDevice::Key::kF7:
				return ImGuiKey_F7;
			case RE::BSKeyboardDevice::Key::kF8:
				return ImGuiKey_F8;
			case RE::BSKeyboardDevice::Key::kF9:
				return ImGuiKey_F9;
			case RE::BSKeyboardDevice::Key::kF10:
				return ImGuiKey_F10;
			case RE::BSKeyboardDevice::Key::kF11:
				return ImGuiKey_F11;
			case RE::BSKeyboardDevice::Key::kF12:
				return ImGuiKey_F12;
			// Skip 584 ~ 595
			case RE::BSKeyboardDevice::Key::kApostrophe:
				return ImGuiKey_Apostrophe;
			case RE::BSKeyboardDevice::Key::kComma:
				return ImGuiKey_Comma;
			case RE::BSKeyboardDevice::Key::kMinus:
				return ImGuiKey_Minus;
			case RE::BSKeyboardDevice::Key::kPeriod:
				return ImGuiKey_Period;
			case RE::BSKeyboardDevice::Key::kSlash:
				return ImGuiKey_Slash;
			case RE::BSKeyboardDevice::Key::kSemicolon:
				return ImGuiKey_Semicolon;
			case RE::BSKeyboardDevice::Key::kEquals:
				return ImGuiKey_Equal;
			case RE::BSKeyboardDevice::Key::kBracketLeft:
				return ImGuiKey_LeftBracket;
			case RE::BSKeyboardDevice::Key::kBackslash:
				return ImGuiKey_Backslash;
			case RE::BSKeyboardDevice::Key::kBracketRight:
				return ImGuiKey_RightBracket;
			case RE::BSKeyboardDevice::Key::kTilde:
				return ImGuiKey_GraveAccent;
			case RE::BSKeyboardDevice::Key::kCapsLock:
				return ImGuiKey_CapsLock;
			case RE::BSKeyboardDevice::Key::kScrollLock:
				return ImGuiKey_ScrollLock;
			case RE::BSKeyboardDevice::Key::kNumLock:
				return ImGuiKey_NumLock;
			case RE::BSKeyboardDevice::Key::kPrintScreen:
				return ImGuiKey_PrintScreen;
			case RE::BSKeyboardDevice::Key::kPause:
				return ImGuiKey_Pause;
			case RE::BSKeyboardDevice::Key::kKP_0:
				return ImGuiKey_Keypad0;
			case RE::BSKeyboardDevice::Key::kKP_1:
				return ImGuiKey_Keypad1;
			case RE::BSKeyboardDevice::Key::kKP_2:
				return ImGuiKey_Keypad2;
			case RE::BSKeyboardDevice::Key::kKP_3:
				return ImGuiKey_Keypad3;
			case RE::BSKeyboardDevice::Key::kKP_4:
				return ImGuiKey_Keypad4;
			case RE::BSKeyboardDevice::Key::kKP_5:
				return ImGuiKey_Keypad5;
			case RE::BSKeyboardDevice::Key::kKP_6:
				return ImGuiKey_Keypad6;
			case RE::BSKeyboardDevice::Key::kKP_7:
				return ImGuiKey_Keypad7;
			case RE::BSKeyboardDevice::Key::kKP_8:
				return ImGuiKey_Keypad8;
			case RE::BSKeyboardDevice::Key::kKP_9:
				return ImGuiKey_Keypad9;
			case RE::BSKeyboardDevice::Key::kKP_Decimal:
				return ImGuiKey_KeypadDecimal;
			case RE::BSKeyboardDevice::Key::kKP_Divide:
				return ImGuiKey_KeypadDivide;
			case RE::BSKeyboardDevice::Key::kKP_Multiply:
				return ImGuiKey_KeypadMultiply;
			case RE::BSKeyboardDevice::Key::kKP_Subtract:
				return ImGuiKey_KeypadSubtract;
			case RE::BSKeyboardDevice::Key::kKP_Plus:
				return ImGuiKey_KeypadAdd;
			case RE::BSKeyboardDevice::Key::kKP_Enter:
				return ImGuiKey_KeypadEnter;
			// Skip 628 ~ 630
			default:
				return ImGuiKey_None;
			}
		}

		static ImGuiKey ParseKeyFromKeyboard(std::uint32_t a_key)
		{
			return ParseKeyFromKeyboard(static_cast<RE::BSKeyboardDevice::Key>(a_key));
		}

		static void ProcessEvent(RE::InputEvent** a_event)
		{
			auto& io = ImGui::GetIO();

			for (auto event = *a_event; event; event = event->next) {
				if (auto charEvent = event->AsCharEvent()) {
					io.AddInputCharacter(charEvent->keycode);
				} else if (auto button = event->AsButtonEvent()) {
					if (!button->HasIDCode())
						continue;

					switch (button->GetDevice()) {
					case RE::INPUT_DEVICE::kKeyboard:
						{
							auto imKey = ParseKeyFromKeyboard(button->GetIDCode());
							auto pressed = button->IsPressed();
							io.AddKeyEvent(imKey, pressed);
						}
						break;
					case RE::INPUT_DEVICE::kMouse:
						{
							switch (auto key = static_cast<RE::BSWin32MouseDevice::Key>(button->GetIDCode())) {
							case RE::BSWin32MouseDevice::Key::kWheelUp:
								io.AddMouseWheelEvent(0, button->Value());
								break;
							case RE::BSWin32MouseDevice::Key::kWheelDown:
								io.AddMouseWheelEvent(0, button->Value() * -1);
								break;
							default:
								io.AddMouseButtonEvent(key, button->IsPressed());
								break;
							}
						}
						break;
					default:
						break;
					}

					/*if (!button || (button->IsPressed() && !button->IsDown()))
						continue;

					auto scan_code = button->GetIDCode();

					using DeviceType = RE::INPUT_DEVICE;
					auto input = scan_code;
					switch (button->device.get()) {
					case DeviceType::kMouse:
						input += 256;
						break;
					case DeviceType::kKeyboard:
						break;
					default:
						continue;
					}

					using DIKey = RE::DirectInput8::DIKey;
					uint32_t key = MapVirtualKeyEx(scan_code, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0));
					switch (scan_code) {
					case DIKey::DIK_LEFTARROW:
						key = VK_LEFT;
						break;
					case DIKey::DIK_RIGHTARROW:
						key = VK_RIGHT;
						break;
					case DIKey::DIK_UPARROW:
						key = VK_UP;
						break;
					case DIKey::DIK_DOWNARROW:
						key = VK_DOWN;
						break;
					case DIKey::DIK_DELETE:
						key = VK_DELETE;
						break;
					case DIKey::DIK_END:
						key = VK_END;
						break;
					case DIKey::DIK_HOME:
						key = VK_HOME;
						break;  // pos1
					case DIKey::DIK_PRIOR:
						key = VK_PRIOR;
						break;  // page up
					case DIKey::DIK_NEXT:
						key = VK_NEXT;
						break;  // page down
					case DIKey::DIK_INSERT:
						key = VK_INSERT;
						break;
					case DIKey::DIK_NUMPAD0:
						key = VK_NUMPAD0;
						break;
					case DIKey::DIK_NUMPAD1:
						key = VK_NUMPAD1;
						break;
					case DIKey::DIK_NUMPAD2:
						key = VK_NUMPAD2;
						break;
					case DIKey::DIK_NUMPAD3:
						key = VK_NUMPAD3;
						break;
					case DIKey::DIK_NUMPAD4:
						key = VK_NUMPAD4;
						break;
					case DIKey::DIK_NUMPAD5:
						key = VK_NUMPAD5;
						break;
					case DIKey::DIK_NUMPAD6:
						key = VK_NUMPAD6;
						break;
					case DIKey::DIK_NUMPAD7:
						key = VK_NUMPAD7;
						break;
					case DIKey::DIK_NUMPAD8:
						key = VK_NUMPAD8;
						break;
					case DIKey::DIK_NUMPAD9:
						key = VK_NUMPAD9;
						break;
					case DIKey::DIK_DECIMAL:
						key = VK_DECIMAL;
						break;
					case DIKey::DIK_NUMPADENTER:
						key = IM_VK_KEYPAD_ENTER;
						break;
					case DIKey::DIK_RMENU:
						key = VK_RMENU;
						break;  // right alt
					case DIKey::DIK_RCONTROL:
						key = VK_RCONTROL;
						break;  // right control
					case DIKey::DIK_LWIN:
						key = VK_LWIN;
						break;  // left win
					case DIKey::DIK_RWIN:
						key = VK_RWIN;
						break;  // right win
					case DIKey::DIK_APPS:
						key = VK_APPS;
						break;
					default:
						break;
					}

					switch (button->device.get()) {
					case RE::INPUT_DEVICE::kMouse:
						if (scan_code > 7)  // middle scroll
							io.AddMouseWheelEvent(0, button->Value() * (scan_code == 8 ? 1 : -1));
						else {
							if (scan_code > 5)
								scan_code = 5;
							io.AddMouseButtonEvent(scan_code, button->IsPressed());
						}
						break;
					case RE::INPUT_DEVICE::kKeyboard:
						io.AddKeyEvent(ImGui_ImplWin32_VirtualKeyToImGuiKey(key), button->IsPressed());
						break;
					default:
						continue;
					}*/
				}
			}
		}

		static void DispatchInputEvent(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent** a_evns)
		{
			static RE::InputEvent* dummy = nullptr;

			bool captured = process(a_evns);
			captured = false;
			if (captured) {
				_DispatchInputEvent(a_dispatcher, &dummy);
			} else {
				if (skipevents()) {
					_DispatchInputEvent(a_dispatcher, &dummy);
					ProcessEvent(a_evns);
				} else {
					_DispatchInputEvent(a_dispatcher, a_evns);
				}
			}
		}

		static LRESULT WndProcHook__thunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			auto& io = ImGui::GetIO();
			if (uMsg == WM_KILLFOCUS) {
				io.ClearInputCharacters();
				io.ClearInputKeys();
			}

			return WndProcHook__func(hWnd, uMsg, wParam, lParam);
		}

		static void Present(uint32_t a1)
		{
			_Present(a1);

			new_frame();
		}

		static inline WNDPROC WndProcHook__func;
		static inline REL::Relocation<decltype(Present)> _Present;
		static inline REL::Relocation<decltype(DispatchInputEvent)> _DispatchInputEvent;
	};
}

namespace FenixUtils
{
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
	auto add_trampoline(Xbyak::CodeGenerator* xbyakCode)
	{
		constexpr REL::ID funcOffset = REL::ID(ID);
		auto funcAddr = funcOffset.address();
		auto size = xbyakCode->getSize();
		auto& trampoline = SKSE::GetTrampoline();
		auto result = trampoline.allocate(size);
		std::memcpy(result, xbyakCode->getCode(), size);
		if constexpr (!call)
			return trampoline.write_branch<BRANCH_TYPE>(funcAddr + offset, (std::uintptr_t)result);
		else
			return trampoline.write_call<BRANCH_TYPE>(funcAddr + offset, (std::uintptr_t)result);
	}

	template <int ID, int offset = 0x0>
	void writebytes(const std::string_view& data)
	{
		REL::safe_write(REL::ID(ID).address() + offset, data.data(), data.size());
	}

	enum class LineOfSightLocation : uint32_t
	{
		kNone,
		kEyes,
		kHead,
		kTorso,
		kFeet
	};
	
	namespace Geom
	{
		// Axis: y -- forward, x -- right, z angle = 0 => y = 1, x = 0.
		// Angles: x:-pi/2 (up)..pi/2 (down), y:0, z:0 (along y)..2pi

		float NiASin(float alpha);

		// --- Vector (dir) to angles ---

		float GetUnclampedZAngleFromVector(const RE::NiPoint3& V);
		float GetZAngleFromVector(const RE::NiPoint3& V);
		void CombatUtilities__GetAimAnglesFromVector(const RE::NiPoint3& V, float& rotZ, float& rotX);
		RE::Projectile::ProjectileRot rot_at(const RE::NiPoint3& dir);
		RE::Projectile::ProjectileRot rot_at(const RE::NiPoint3& from, const RE::NiPoint3& to);

		// ^^^ Vector (dir) to angles ^^^

		// --- rotations ---

		RE::NiPoint3 angles2dir(const RE::NiPoint3& angles);
		// Get vector with len=r, with euler=angles
		RE::NiPoint3 rotate(float r, const RE::NiPoint3& angles);
		RE::NiPoint3 rotate(const RE::NiPoint3& A, const RE::NiPoint3& angles);
		RE::NiPoint3 rotate(const RE::NiPoint3& P, float alpha, const RE::NiPoint3& axis_dir);
		RE::NiPoint3 rotate(const RE::NiPoint3& P, float alpha, const RE::NiPoint3& origin, const RE::NiPoint3& axis_dir);
		// rotate vel_cur to dir_final with max angle max_alpha
		RE::NiPoint3 rotateVel(const RE::NiPoint3& vel_cur, float max_alpha, const RE::NiPoint3& dir_final);

		// ^^^ rotations ^^^

		namespace Projectile
		{
			void update_node_rotation(RE::Projectile* proj, const RE::NiPoint3& V);
			void update_node_rotation(RE::Projectile* proj);
			void aimToPoint(RE::Projectile* proj, const RE::NiPoint3& P);
		}

		namespace Actor
		{
			RE::NiPoint3 CalculateLOSLocation(RE::TESObjectREFR* refr, LineOfSightLocation los_loc);
			// get point `caster` is looking at
			RE::NiPoint3 raycast(RE::Actor* caster);
			RE::NiPoint3 AnticipatePos(RE::Actor* a, float dtime = 0);
			LineOfSightLocation Actor__CalculateLOS(RE::Actor* caster, RE::Actor* target, float viewCone);
			bool ActorInLOS(RE::Actor* caster, RE::Actor* target, float viewCone);
			// get polar_angle from los a to target_pos
			float Actor__CalculateAimDelta(RE::Actor* a, const RE::NiPoint3& target_pos);
			// get CalculateAimDelta and compare it with viewCone / 2
			bool Actor__IsPointInAimCone(RE::Actor* a, RE::NiPoint3* target_pos, float viewCone);
		}
	}

	namespace Random
	{
		// random(0..1) <= chance
		bool RandomBoolChance(float prop);
		float Float(float min, float max);
		float FloatChecked(float min, float max);
		float FloatTwoPi();
		float Float0To1();
		float FloatNeg1To1();
		int32_t random_range(int32_t min, int32_t max);
	}

	namespace Json
	{
		// Returns mod mask number or -1 if missing
		int get_mod_index(std::string_view name);
		// Returns number from `Skyrim.esm|0x1000` or `0x10100000`
		uint32_t get_formid(const std::string& name);

		// --- getting values ---

		RE::NiPoint3 getPoint3(const ::Json::Value& jobj, const std::string& field_name);
		template <RE::NiPoint3 default_val = RE::NiPoint3(0, 0, 0)>
		RE::NiPoint3 mb_getPoint3(const ::Json::Value& jobj, const std::string& field_name)
		{
			return jobj.isMember(field_name) ? getPoint3(jobj, field_name) : default_val;
		}
		RE::Projectile::ProjectileRot getPoint2(const ::Json::Value& jobj, const std::string& field_name);
		RE::Projectile::ProjectileRot mb_getPoint2(const ::Json::Value& jobj, const std::string& field_name);
		std::string getString(const ::Json::Value& jobj, const std::string& field_name);
		std::string mb_getString(const ::Json::Value& jobj, const std::string& field_name);
		float getFloat(const ::Json::Value& jobj, const std::string& field_name);
		template <float default_val = 0.0f>
		float mb_getFloat(const ::Json::Value& jobj, const std::string& field_name)
		{
			return jobj.isMember(field_name) ? getFloat(jobj, field_name) : default_val;
		}
		bool getBool(const ::Json::Value& jobj, const std::string& field_name);
		uint32_t getUint32(const ::Json::Value& jobj, const std::string& field_name);

		template <typename Enum>
		Enum string2enum(const std::string& val)
		{
			return magic_enum::enum_cast<Enum>(val).value();
		}

		// Get string data from val and return enum
		template <typename Enum>
		Enum read_enum(const ::Json::Value& val)
		{
			return string2enum<Enum>(val.asString());
		}

		// Get string data from jobj[field_name] and return enum
		template <typename Enum>
		Enum read_enum(const ::Json::Value& jobj, const std::string& field_name)
		{
			assert(jobj.isMember(field_name));
			return read_enum<Enum>(jobj[field_name]);
		}

		// Get data from jobj[field_name], if present, return def otherwise
		template <auto def>
		auto mb_read_field(const ::Json::Value& jobj, const std::string& field_name)
		{
			if constexpr (std::is_same_v<decltype(def), bool>) {
				if (jobj.isMember(field_name))
					return getBool(jobj, field_name);
				else
					return def;
			} else if constexpr (std::is_same_v<decltype(def), uint32_t>) {
				if (jobj.isMember(field_name))
					return getUint32(jobj, field_name);
				else
					return def;
			} else {
				if (jobj.isMember(field_name))
					return read_enum<decltype(def)>(jobj, field_name);
				else
					return def;
			}
		}

		// ^^^ getting values ^^^
	}

	namespace Timer
	{
		float get_time();
		bool passed(float timestamp, float interval);
		bool passed(RE::AITimer& timer);
		void updateAndWait(RE::AITimer& timer, float interval);
	}

	namespace Behavior
	{
		RE::hkbNode* lookup_node(RE::hkbBehaviorGraph* root_graph, const char* name);
		int32_t get_implicit_id_variable(RE::hkbBehaviorGraph* graph, const char* name);
		RE::hkbEventBase::SystemEventIDs get_implicit_id_event(RE::hkbBehaviorGraph* graph, const char* name);
		const char* get_event_name_implicit(RE::hkbBehaviorGraph* graph, RE::hkbEventBase::SystemEventIDs implicit_id);
		const char* get_event_name_explicit(RE::hkbBehaviorGraph* graph, int32_t explicit_id);
		const char* get_variable_name(RE::hkbBehaviorGraph* graph, int32_t ind);
	}


	float Projectile__GetSpeed(RE::Projectile* proj);
	void Projectile__set_collision_layer(RE::Projectile* proj, RE::COL_LAYER collayer);

	void TESObjectREFR__SetAngleOnReferenceX(RE::TESObjectREFR* refr, float angle_x);
	void TESObjectREFR__SetAngleOnReferenceZ(RE::TESObjectREFR* refr, float angle_z);

	RE::TESObjectARMO* GetEquippedShield(RE::Actor* a);
	RE::EffectSetting* getAVEffectSetting(RE::MagicItem* mgitem);

	void damage_stamina_withdelay(RE::Actor* a, float val);
	void damageav_attacker(RE::Actor* victim, RE::ACTOR_VALUE_MODIFIERS::ACTOR_VALUE_MODIFIER i1, RE::ActorValue i2, float val, RE::Actor* attacker);
	void damageav(RE::Actor* a, RE::ActorValue av, float val);
	RE::TESObjectWEAP* get_UnarmedWeap();
	float PlayerCharacter__get_reach(RE::Actor* a);
	float GetHeadingAngle(RE::TESObjectREFR* a, const RE::NiPoint3& a_pos, bool a_abs);
	void UnequipItem(RE::Actor* a, RE::TESBoundObject* item);
	void knock(RE::Actor* target, RE::Actor* aggressor, float KnockDown);
	void cast_spell(RE::Actor* victim, RE::Actor* attacker, RE::SpellItem* spell);
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
	float get_dist2(RE::TESObjectREFR* a, RE::TESObjectREFR* b);


	RE::BGSAttackDataPtr get_attackData(RE::Actor* a);
	bool is_powerattacking(RE::Actor* a);
	RE::TESObjectWEAP* get_weapon(RE::Actor* a);
	bool is_human(RE::Actor* a);
	void set_RegenDelay(RE::AIProcess* proc, RE::ActorValue av, float time);
	void FlashHudMenuMeter__blink(RE::ActorValue av);
	float get_regen(RE::Actor* a, RE::ActorValue av);
	void damagestamina_delay_blink(RE::Actor* a, float cost);
	float getAV_pers(RE::Actor* a, RE::ActorValue av);
	float lerp(float x, float Ax, float Ay, float Bx, float By);

	void AddItem(RE::Actor* a, RE::TESBoundObject* item, RE::ExtraDataList* extraList, int count, RE::TESObjectREFR* fromRefr);
	void AddItemPlayer(RE::TESBoundObject* item, int count);
	int RemoveItemPlayer(RE::TESBoundObject* item, int count);

	int get_item_count(RE::Actor* a, RE::TESBoundObject* item);
	uint32_t* placeatme(RE::TESObjectREFR* a, uint32_t* handle, RE::TESBoundObject* form, RE::NiPoint3* pos, RE::NiPoint3* angle);
	RE::TESObjectREFR* placeatmepap(RE::TESObjectREFR* a, RE::TESBoundObject* form, int count);

	template <typename... Args>
	void notification(const char* format, Args... a)
	{
		char sMsgBuff[128] = { 0 };
		sprintf_s(sMsgBuff, format, a...);
		RE::DebugNotification(sMsgBuff);
	}

	template <typename... Args>
	std::string vformat(const std::string_view& format, Args&&... args)
	{
		try {
			return std::vformat(format, std::make_format_args(std::forward<Args>(args)...));
		} catch (...) {
			return "";
		}
	}

	template <typename... Args>
	void notification(const std::string_view& format, Args&&... a)
	{
		RE::DebugNotification(vformat(format, std::forward<Args>(a)...).c_str());
	}

	template <typename... Args>
	void messagebox(const std::string_view& format, Args&&... a)
	{
		RE::DebugMessageBox(vformat(format, std::forward<Args>(a)...).c_str());
	}

	template <typename Key, typename Value, std::size_t Size>
	struct Map
	{
		std::array<std::pair<Key, Value>, Size> data;

		[[nodiscard]] constexpr Value at(const Key& key) const
		{
			const auto itr = std::find_if(begin(data), end(data), [&key](const auto& v) { return v.first == key; });
			if (itr != end(data)) {
				return itr->second;
			} else {
				throw std::range_error("Not Found");
			}
		}

		[[nodiscard]] constexpr bool contains_value(const Value& val) const
		{
			const auto itr = std::find_if(begin(data), end(data), [&val](const auto& v) { return v.second == val; });
			return itr != end(data);
		}
	};

	bool is_playable_spel(RE::SpellItem* spel);
}
using FenixUtils::_generic_foo_;
using FenixUtils::add_trampoline;

constexpr uint32_t hash(const char* data, size_t const size) noexcept
{
	uint32_t hash = 5381;

	for (const char* c = data; c < data + size; ++c) {
		hash = ((hash << 5) + hash) + (unsigned char)*c;
	}

	return hash;
}

constexpr char to_lower(char c)
{
	if (c >= 'A' && c <= 'Z')
		c += 32;
	return c;
}

constexpr uint32_t hash_lowercase(const char* data, size_t const size) noexcept
{
	uint32_t hash = 5381;

	for (const char* c = data; c < data + size; ++c) {
		hash = ((hash << 5) + hash) + (unsigned char)to_lower(static_cast<unsigned char>(*c));
	}

	return hash;
}

constexpr uint32_t operator"" _h(const char* str, size_t size) noexcept
{
	return hash(str, size);
}

constexpr uint32_t operator"" _hl(const char* str, size_t size) noexcept
{
	return hash_lowercase(str, size);
}

#include "SimpleIni.h"
class SettingsBase
{
protected:
	static bool ReadBool(const CSimpleIniA& ini, const char* section, const char* setting, bool& ans)
	{
		if (ini.GetValue(section, setting)) {
			ans = ini.GetBoolValue(section, setting);
			return true;
		}
		return false;
	}

	static bool ReadFloat(const CSimpleIniA& ini, const char* section, const char* setting, float& ans)
	{
		if (ini.GetValue(section, setting)) {
			ans = static_cast<float>(ini.GetDoubleValue(section, setting));
			return true;
		}
		return false;
	}

	static bool ReadUint32(const CSimpleIniA& ini, const char* section, const char* setting, uint32_t& ans)
	{
		if (ini.GetValue(section, setting)) {
			ans = static_cast<uint32_t>(ini.GetLongValue(section, setting));
			return true;
		}
		return false;
	}

	static bool ReadString(const CSimpleIniA& ini, const char* section, const char* setting, std::string& ans)
	{
		if (ini.GetValue(section, setting)) {
			auto tmp = ini.GetValue(section, setting);
			ans = std::string(tmp);
			return true;
		}
		return false;
	}
};
