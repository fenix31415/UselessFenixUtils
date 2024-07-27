#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <dxgi.h>

namespace ImguiUtils
{
	template <void (*show)(), bool (*is_hide_hotkey)(RE::ButtonEvent* b), bool (*is_enable_hotkey)(RE::ButtonEvent* b)>
	class ImGuiHelper
	{
		static inline std::atomic<bool> IsOpen;
		static inline std::atomic<bool> IsActive;

		static void toggle_IsOpen()
		{
			bool is_open_new = !IsOpen.load();
			IsOpen = is_open_new;
			if (is_open_new)
				IsActive = false;
		}
		static void toggle_IsActive() { IsActive = !IsActive.load(); }

		static void update_cursor_status() { ImGui::GetIO().MouseDrawCursor = IsActive.load() && IsOpen.load(); }

		static void Process(RE::InputEvent* const* evns)
		{
			if (!*evns)
				return;

			for (RE::InputEvent* e = *evns; e; e = e->next) {
				if (auto b = e->AsButtonEvent()) {
					if (!b->IsDown() || b->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
						continue;

					if (is_hide_hotkey(b)) {
						toggle_IsOpen();
						update_cursor_status();
					}
					if (is_enable_hotkey(b)) {
						toggle_IsActive();
						update_cursor_status();
					}
				}
			}
		}

		static bool skipevents() { return IsActive.load() && IsOpen.load(); }

	public:
		static void Initialize()
		{
			IsOpen = false;
			IsActive = false;

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

			if (IsOpen)
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
				}
			}
		}

		static void DispatchInputEvent(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent** a_evns)
		{
			static RE::InputEvent* dummy = nullptr;

			Process(a_evns);
			if (skipevents()) {
				_DispatchInputEvent(a_dispatcher, &dummy);
				ProcessEvent(a_evns);
			} else {
				_DispatchInputEvent(a_dispatcher, a_evns);
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
