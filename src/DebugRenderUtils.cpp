#include <d3dcompiler.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#include "UselessDebugRenderUtils.h"
#include <d3d11.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <winrt/base.h>
#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#pragma warning(push)
#pragma warning(disable: 4267)  // conversion from 'size_t' to 'type', possible loss of data
#pragma warning(disable: 4244)  // conversion from 'type1' to 'type2', possible loss of data

static uintptr_t g_worldToCamMatrix = RELOCATION_ID(519579, 406126).address();                       // 2F4C910, 2FE75F0
static RE::NiRect<float>* g_viewPort = (RE::NiRect<float>*)RELOCATION_ID(519618, 406160).address();  // 2F4DED0, 2FE8B98

#pragma pack(push, 1)
struct DDS_PIXELFORMAT
{
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwFourCC;
	uint32_t dwRGBBitCount;
	uint32_t dwRBitMask;
	uint32_t dwGBitMask;
	uint32_t dwBBitMask;
	uint32_t dwABitMask;
};

struct DDS_HEADER
{
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwHeight;
	uint32_t dwWidth;
	uint32_t dwPitchOrLinearSize;
	uint32_t dwDepth;
	uint32_t dwMipMapCount;
	uint32_t dwReserved1[11];
	DDS_PIXELFORMAT ddspf;
	uint32_t dwCaps;
	uint32_t dwCaps2;
	uint32_t dwCaps3;
	uint32_t dwCaps4;
	uint32_t dwReserved2;
};
#pragma pack(pop)

const uint32_t DDS_MAGIC = 0x20534444;  // "DDS "

namespace DebugRenderUtils
{
	class ITimer
	{
	public:
		ITimer() : m_qpcBase(0), m_tickBase(0) { Init(); }

		~ITimer() {}

		static void Init(void)
		{
			if (!s_secondsPerCount) {
				// init qpc
				uint64_t countsPerSecond;
				[[maybe_unused]] BOOL res = QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSecond);

				//ASSERT_STR(res, "ITimer: no high-resolution timer support");

				s_secondsPerCount = 1.0 / countsPerSecond;

				s_qpcWrapMargin = (uint64_t)(-(
					(int64_t)(countsPerSecond *
							  60)));  // detect if we've wrapped around by a delta greater than this - also limits max time
				//_MESSAGE("s_qpcWrapMargin: %016I64X", s_qpcWrapMargin);
				//_MESSAGE("wrap time: %fs", ((double)0xFFFFFFFFFFFFFFFF) * s_secondsPerCount);

				// init multimedia timer
				timeGetDevCaps(&s_timecaps, sizeof(s_timecaps));

				//_MESSAGE("min timer period = %d", s_timecaps.wPeriodMin);

				s_setTime = (timeBeginPeriod(s_timecaps.wPeriodMin) == TIMERR_NOERROR);
				/*if(!s_setTime)
			_WARNING("couldn't change timer period");*/
			}
		}

		static void DeInit(void)
		{
			if (s_secondsPerCount) {
				if (s_setTime) {
					timeEndPeriod(s_timecaps.wPeriodMin);
					s_setTime = false;
				}

				/*if(s_qpcWrapCount)
			_MESSAGE("s_qpcWrapCount: %d", s_qpcWrapCount);*/

				s_secondsPerCount = 0;
			}
		}

		void Start(void)
		{
			m_qpcBase = GetQPC();
			m_tickBase = timeGetTime();
		}

		// seconds
		double GetElapsedTime(void)
		{
			uint64_t qpcNow = GetQPC();
			uint32_t tickNow = timeGetTime();

			uint64_t qpcDelta = qpcNow - m_qpcBase;
			uint64_t tickDelta = tickNow - m_tickBase;

			double qpcSeconds = ((double)qpcDelta) * s_secondsPerCount;
			double tickSeconds = ((double)tickDelta) * 0.001;  // ticks are in milliseconds
			double qpcTickDelta = qpcSeconds - tickSeconds;

			if (qpcTickDelta < 0)
				qpcTickDelta = -qpcTickDelta;

			// if they differ by more than one second, something's wrong, return
			if (qpcTickDelta > 1) {
				s_qpcInaccurateCount++;
				return tickSeconds;
			} else {
				return qpcSeconds;
			}
		}

	private:
		uint64_t m_qpcBase;   // QPC
		uint32_t m_tickBase;  // timeGetTime

		static inline double s_secondsPerCount = 0;
		static inline TIMECAPS s_timecaps = { 0 };
		static inline bool s_setTime = 0;

		// safe QPC stuff
		static uint64_t GetQPC(void)
		{
			uint64_t now;

			QueryPerformanceCounter((LARGE_INTEGER*)&now);

			if (s_hasLastQPC) {
				uint64_t delta = now - s_lastQPC;

				if (delta > s_qpcWrapMargin) {
					// we've gone back in time, return a kludged value

					s_lastQPC = now;
					now = s_lastQPC + 1;

					s_qpcWrapCount++;
				} else {
					s_lastQPC = now;
				}
			} else {
				s_hasLastQPC = true;
				s_lastQPC = now;
			}

			return now;
		}

		static inline uint64_t s_lastQPC = 0;
		static inline uint64_t s_qpcWrapMargin = 0;
		static inline bool s_hasLastQPC = false;

		static inline uint32_t s_qpcWrapCount = 0;
		static inline uint32_t s_qpcInaccurateCount = 0;
	};

	static ITimer timer;
	static double curFrame = 0.0;
	static double lastFrame = 0.0;
	static double curQPC = 0.0;
	static double lastQPC = 0.0;

	namespace GameTime
	{
		void Initialize() noexcept { timer.Start(); }

		double GetTime() noexcept { return timer.GetElapsedTime(); }

		double GetQPC() noexcept
		{
			LARGE_INTEGER f, i;
			if (QueryPerformanceCounter(&i) && QueryPerformanceFrequency(&f)) {
				auto frequency = 1.0 / static_cast<double>(f.QuadPart);
				return static_cast<double>(i.QuadPart) * frequency;
			}
			return 0.0;
		}

		void StepFrameTime() noexcept
		{
			lastFrame = curFrame;
			curFrame = GetTime();

			lastQPC = curQPC;
			curQPC = GetQPC();
		}

		double CurTime() noexcept { return curFrame; }

		double CurQPC() noexcept { return curQPC; }

		double GetFrameDelta() noexcept { return curFrame - lastFrame; }

		double GetQPCDelta() noexcept { return curQPC - lastQPC; }
	}

	class VertexBuffer;

	enum class PipelineStage
	{
		Vertex,
		Fragment,
	};

	struct ShaderCreateInfo
	{
		std::string source;
		std::string entryName = "main";
		std::string version;
		PipelineStage stage;

		ShaderCreateInfo(std::string&& source, PipelineStage stage, std::string&& entryName = "main",
			std::string&& version = "5_0") : source(source), entryName(entryName), version(version), stage(stage)
		{}
	};

	typedef struct _D3DContext
	{
		// @Note: we don't want to refCount the swap chain - Let skyrim manage the lifetime.
		// As long as the game is running, we have a valid swapchain.
		IDXGISwapChain* swapChain = nullptr;
		winrt::com_ptr<ID3D11Device> device;
		winrt::com_ptr<ID3D11DeviceContext> context;
		// Size of the output window in pixels
		glm::vec2 windowSize = {};

		ID3D11Device* GetDevice() const { return device.get(); }
		ID3D11DeviceContext* GetContext() const { return context.get(); }
	} D3DContext;

	static D3DContext gameContext;

	using DrawFunc = std::function<void(D3DContext&)>;
	static std::vector<DrawFunc> presentCallbacks;
	static bool initialized = false;

	// Ideally you would do something a bit more clever, but with what little rendering we do storing a lazy cache like this is fine
	// Depth
	typedef struct DSStateKey
	{
		bool write;
		bool test;
		D3D11_COMPARISON_FUNC mode;

		DSStateKey(bool write, bool test, D3D11_COMPARISON_FUNC mode) : write(write), test(test), mode(mode) {}

		size_t Hash() const { return std::hash<bool>()(write) ^ std::hash<bool>()(test) ^ std::hash<uint16_t>()(mode); }

		bool operator==(const DSStateKey& other) const
		{
			return write == other.write && test == other.test && mode == other.mode;
		}
	} DSStateKey;

	typedef struct DSState
	{
		winrt::com_ptr<ID3D11DepthStencilState> state;

		DSState(D3DContext& ctx, DSStateKey& info)
		{
			D3D11_DEPTH_STENCIL_DESC dsDesc;
			dsDesc.DepthEnable = info.write;
			dsDesc.DepthWriteMask = info.test ? D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ALL :
			                                    D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ZERO;
			dsDesc.DepthFunc = info.mode;
			dsDesc.StencilEnable = false;
			ctx.device->CreateDepthStencilState(&dsDesc, state.put());
		}

		~DSState() {}

		DSState(const DSState&) = delete;
		DSState(DSState&& loc)
		{
			// Doing this, we don't do extra ref counting
			state.attach(loc.state.get());
			loc.state.detach();
		};
		DSState& operator=(const DSState&) = delete;
		DSState& operator=(DSState&& loc)
		{
			// Doing this, we don't do extra ref counting
			state.attach(loc.state.get());
			loc.state.detach();
		};
	} DSState;

	struct DSHasher
	{
		size_t operator()(const DSStateKey& key) const { return key.Hash(); }
	};

	struct DSCompare
	{
		size_t operator()(const DSStateKey& k1, const DSStateKey& k2) const { return k1 == k2; }
	};

	// Fixed function blending
	typedef struct BlendStateKey
	{
		D3D11_BLEND_DESC desc;
		float factors[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

		BlendStateKey() { ZeroMemory(&desc, sizeof(D3D11_BLEND_DESC)); }

		size_t HashRTBlendDesc(const D3D11_RENDER_TARGET_BLEND_DESC& rtDesc) const
		{
			return std::hash<bool>()(rtDesc.BlendEnable) ^ std::hash<uint8_t>()(rtDesc.SrcBlend) ^
			       std::hash<uint8_t>()(rtDesc.DestBlend) ^ std::hash<uint8_t>()(rtDesc.SrcBlendAlpha) ^
			       std::hash<uint8_t>()(rtDesc.DestBlendAlpha) ^ std::hash<uint8_t>()(rtDesc.BlendOp) ^
			       std::hash<uint8_t>()(rtDesc.BlendOpAlpha) ^ std::hash<uint8_t>()(rtDesc.RenderTargetWriteMask);
		}

		size_t Hash() const
		{
			return std::hash<bool>()(desc.AlphaToCoverageEnable) ^ std::hash<bool>()(desc.IndependentBlendEnable) ^
			       HashRTBlendDesc(desc.RenderTarget[0]);  // @NOTE: We only work with the main RT right now
		}

		bool RTBlendDescEq(const D3D11_RENDER_TARGET_BLEND_DESC& other) const
		{
			return desc.RenderTarget[0].BlendEnable == other.BlendEnable && desc.RenderTarget[0].BlendOp == other.BlendOp &&
			       desc.RenderTarget[0].BlendOpAlpha == other.BlendOpAlpha && desc.RenderTarget[0].DestBlend == other.DestBlend &&
			       desc.RenderTarget[0].DestBlendAlpha == other.DestBlendAlpha &&
			       desc.RenderTarget[0].RenderTargetWriteMask == other.RenderTargetWriteMask &&
			       desc.RenderTarget[0].SrcBlend == other.SrcBlend && desc.RenderTarget[0].SrcBlendAlpha == other.SrcBlendAlpha;
		}

		bool operator==(const BlendStateKey& other) const
		{
			return desc.AlphaToCoverageEnable == other.desc.AlphaToCoverageEnable &&
			       desc.IndependentBlendEnable == other.desc.IndependentBlendEnable && RTBlendDescEq(other.desc.RenderTarget[0]);
		}
	} BlendStateKey;

	struct BlendStateHasher
	{
		size_t operator()(const BlendStateKey& key) const { return key.Hash(); }
	};

	struct BlendStateCompare
	{
		size_t operator()(const BlendStateKey& k1, const BlendStateKey& k2) const { return k1 == k2; }
	};

	typedef struct BlendState
	{
		winrt::com_ptr<ID3D11BlendState> state;

		BlendState(D3DContext& ctx, BlendStateKey& info) { ctx.device->CreateBlendState(&info.desc, state.put()); }

		~BlendState() {}

		BlendState(const BlendState&) = delete;
		BlendState(BlendState&& loc)
		{
			// Doing this, we don't do extra ref counting
			state.attach(loc.state.get());
			loc.state.detach();
		};
		BlendState& operator=(const BlendState&) = delete;
		BlendState& operator=(BlendState&& loc)
		{
			// Doing this, we don't do extra ref counting
			state.attach(loc.state.get());
			loc.state.detach();
		}
	} BlendState;

	// Rasterizer
	typedef struct RasterStateKey
	{
		D3D11_RASTERIZER_DESC desc;

		RasterStateKey(D3D11_RASTERIZER_DESC desc) : desc(desc) {}

		size_t Hash() const
		{
			return std::hash<uint8_t>()(desc.FillMode) ^ std::hash<uint8_t>()(desc.CullMode) ^
			       std::hash<bool>()(desc.FrontCounterClockwise) ^ std::hash<uint32_t>()(desc.DepthBias) ^
			       std::hash<float>()(desc.DepthBiasClamp) ^ std::hash<float>()(desc.SlopeScaledDepthBias) ^
			       std::hash<bool>()(desc.DepthClipEnable) ^ std::hash<bool>()(desc.ScissorEnable) ^
			       std::hash<bool>()(desc.MultisampleEnable) ^ std::hash<bool>()(desc.AntialiasedLineEnable);
		}

		bool operator==(const RasterStateKey& other) const
		{
			return desc.FillMode == other.desc.FillMode && desc.CullMode == other.desc.CullMode &&
			       desc.FrontCounterClockwise == other.desc.FrontCounterClockwise && desc.DepthBias == other.desc.DepthBias &&
			       desc.DepthBiasClamp == other.desc.DepthBiasClamp && desc.DepthClipEnable == other.desc.DepthClipEnable &&
			       desc.ScissorEnable == other.desc.ScissorEnable && desc.MultisampleEnable == other.desc.MultisampleEnable &&
			       desc.AntialiasedLineEnable == other.desc.AntialiasedLineEnable;
		}
	} RasterStateKey;

	typedef struct RasterState
	{
		winrt::com_ptr<ID3D11RasterizerState> state;

		RasterState(D3DContext& ctx, RasterStateKey& info) { ctx.device->CreateRasterizerState(&info.desc, state.put()); }

		~RasterState() {}

		RasterState(const RasterState&) = delete;
		RasterState(RasterState&& loc)
		{
			// Doing this, we don't do extra ref counting
			state.attach(loc.state.get());
			loc.state.detach();
		};
		RasterState& operator=(const RasterState&) = delete;
		RasterState& operator=(RasterState&& loc)
		{
			// Doing this, we don't do extra ref counting
			state.attach(loc.state.get());
			loc.state.detach();
		};
	} RasterState;

	struct RasterStateHasher
	{
		size_t operator()(const RasterStateKey& key) const { return key.Hash(); }
	};

	struct RasterStateCompare
	{
		size_t operator()(const RasterStateKey& k1, const RasterStateKey& k2) const { return k1 == k2; }
	};

	struct D3DObjectsStore
	{
		winrt::com_ptr<ID3D11DepthStencilView> depthStencilView;
		winrt::com_ptr<ID3D11RenderTargetView> gameRTV;

		std::unordered_map<DSStateKey, DSState, DSHasher, DSCompare> loadedDepthStates;

		std::unordered_map<BlendStateKey, BlendState, BlendStateHasher, BlendStateCompare> loadedBlendStates;

		std::unordered_map<RasterStateKey, RasterState, RasterStateHasher, RasterStateCompare> loadedRasterStates;

		void release()
		{
			depthStencilView = nullptr;
			gameRTV = nullptr;
			loadedRasterStates.clear();
			loadedDepthStates.clear();
			loadedBlendStates.clear();
		}
	};
	static D3DObjectsStore d3dObjects;

	// The present hook
	typedef HRESULT (*D3D11Present)(IDXGISwapChain*, UINT, UINT);

	static bool ReadSwapChain()
	{
		// Hopefully SEH will shield us from unexpected crashes with other mods/programs
		__try {
			struct UnkCreationD3D
			{
				uintptr_t unk0;
				uintptr_t unk1;
				uintptr_t unk2;
				IDXGISwapChain* swapChain;
			};
			//auto data = **Offsets::Get<UnkCreationD3D**>(524730);
			auto data = **(UnkCreationD3D**)(RELOCATION_ID(524730, 411349).address());
			// Naked pointer to the swap chain
			gameContext.swapChain = data.swapChain;

			// Try and read the desc as a simple test
			DXGI_SWAP_CHAIN_DESC desc;
			if (!SUCCEEDED(data.swapChain->GetDesc(&desc)))
				return false;

			// Read our screen size
			RECT r;
			GetClientRect(desc.OutputWindow, &r);
			gameContext.windowSize.x = static_cast<float>(r.right);
			gameContext.windowSize.y = static_cast<float>(r.bottom);
		} __except (1) {
			return false;
		}
		return true;
	}

	HRESULT Present(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
	static inline REL::Relocation<decltype(Present)> fnPresentOrig;
	HRESULT Present(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
	{
		// Save some context state to restore later
		winrt::com_ptr<ID3D11DepthStencilState> gameDSState;
		uint32_t gameStencilRef;
		gameContext.context->OMGetDepthStencilState(gameDSState.put(), &gameStencilRef);

		winrt::com_ptr<ID3D11BlendState> gameBlendState;
		float gameBlendFactors[4];
		uint32_t gameSampleMask;
		gameContext.context->OMGetBlendState(gameBlendState.put(), gameBlendFactors, &gameSampleMask);

		D3D11_VIEWPORT gamePort;
		uint32_t numPorts = 1;
		gameContext.context->RSGetViewports(&numPorts, &gamePort);

		D3D11_VIEWPORT port;
		port.TopLeftX = gamePort.TopLeftX;
		port.TopLeftY = gamePort.TopLeftY;
		port.Width = gamePort.Width;
		port.Height = gamePort.Height;
		port.MinDepth = 0;
		port.MaxDepth = 1;
		gameContext.context->RSSetViewports(1, &port);

		winrt::com_ptr<ID3D11RasterizerState> rasterState;
		gameContext.context->RSGetState(rasterState.put());

		gameContext.context->OMGetRenderTargets(1, d3dObjects.gameRTV.put(), d3dObjects.depthStencilView.put());

		{
			for (auto& cb : presentCallbacks) cb(gameContext);

			auto color = d3dObjects.gameRTV.get();
			gameContext.context->OMSetRenderTargets(1, &color, d3dObjects.depthStencilView.get());
		}

		// Put things back the way we found it
		auto ptrRTV = d3dObjects.gameRTV.get();
		gameContext.context->OMSetRenderTargets(1, &ptrRTV, d3dObjects.depthStencilView.get());
		gameContext.context->RSSetState(rasterState.get());
		gameContext.context->RSSetViewports(1, &gamePort);
		gameContext.context->OMSetBlendState(gameBlendState.get(), gameBlendFactors, gameSampleMask);
		gameContext.context->OMSetDepthStencilState(gameDSState.get(), gameStencilRef);

		d3dObjects.depthStencilView = nullptr;
		d3dObjects.gameRTV = nullptr;

		//return reinterpret_cast<Render::D3D11Present>(origVFuncs_D3D[8])(swapChain, syncInterval, flags);
		return fnPresentOrig(swapChain, syncInterval, flags);
	}

	// Install D3D hooks
	void InstallHooks()
	{
		ReadSwapChain();
		gameContext.swapChain->GetDevice(__uuidof(ID3D11Device), gameContext.device.put_void());

		REL::Relocation<std::uintptr_t> D3DVtbl{ *(uintptr_t*)gameContext.swapChain };
		fnPresentOrig = D3DVtbl.write_vfunc(0x8, Present);

		gameContext.device->GetImmediateContext(gameContext.context.put());
		initialized = true;
	}

	// Get the game's D3D context
	D3DContext& GetContext() noexcept { return gameContext; }

	// Returns true if we have a valid D3D context
	bool HasContext() noexcept { return initialized; }

	// Add a new function for drawing during the present hook
	void OnPresent(DrawFunc&& callback) { presentCallbacks.emplace_back(callback); }

	// Set the depth state
	void SetDepthState(D3DContext& ctx, bool writeEnable, bool testEnable, D3D11_COMPARISON_FUNC testFunc)
	{
		auto key = DSStateKey{ writeEnable, testEnable, testFunc };
		auto it = d3dObjects.loadedDepthStates.find(key);
		if (it != d3dObjects.loadedDepthStates.end()) {
			ctx.context->OMSetDepthStencilState(it->second.state.get(), 255);
			return;
		}

		auto state = DSState{ ctx, key };
		ctx.context->OMSetDepthStencilState(state.state.get(), 255);
		d3dObjects.loadedDepthStates.emplace(key, std::move(state));
	}

	// Set the blending state
	void SetBlendState(D3DContext& ctx, bool enable, D3D11_BLEND_OP blendOp = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP blendAlphaOp = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD, D3D11_BLEND src = D3D11_BLEND::D3D11_BLEND_ONE,
		D3D11_BLEND dest = D3D11_BLEND::D3D11_BLEND_ZERO, D3D11_BLEND srcAlpha = D3D11_BLEND::D3D11_BLEND_ONE,
		D3D11_BLEND destAlpha = D3D11_BLEND::D3D11_BLEND_ZERO, bool alphaToCoverage = false)
	{
		auto key = BlendStateKey{};
		key.desc.AlphaToCoverageEnable = alphaToCoverage;
		key.desc.IndependentBlendEnable = false;
		key.desc.RenderTarget[0].BlendEnable = enable;
		key.desc.RenderTarget[0].BlendOp = blendOp;
		key.desc.RenderTarget[0].BlendOpAlpha = blendAlphaOp;
		key.desc.RenderTarget[0].SrcBlend = src;
		key.desc.RenderTarget[0].DestBlend = dest;
		key.desc.RenderTarget[0].SrcBlendAlpha = srcAlpha;
		key.desc.RenderTarget[0].DestBlendAlpha = destAlpha;
		key.desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE::D3D11_COLOR_WRITE_ENABLE_ALL;

		auto it = d3dObjects.loadedBlendStates.find(key);
		if (it != d3dObjects.loadedBlendStates.end()) {
			ctx.context->OMSetBlendState(it->second.state.get(), key.factors, 0xffffffff);
			return;
		}

		auto state = BlendState{ ctx, key };
		ctx.context->OMSetBlendState(state.state.get(), key.factors, 0xffffffff);

		d3dObjects.loadedBlendStates.emplace(key, std::move(state));
	}

	class Shader
	{
	public:
		Shader(const ShaderCreateInfo& createInfo, D3DContext& ctx) noexcept : stage(createInfo.stage), context(ctx)
		{
			validBinary = Compile(createInfo.source, createInfo.entryName, createInfo.version);
			if (!validBinary)
				return;

			if (stage == PipelineStage::Vertex) {
				auto result = context.device->CreateVertexShader(binary->GetBufferPointer(), binary->GetBufferSize(), nullptr,
					&program.vertex);
				validProgram = SUCCEEDED(result);
				if (!validProgram)
					logger::error("A shader failed to compile.");
			} else {
				auto result = context.device->CreatePixelShader(binary->GetBufferPointer(), binary->GetBufferSize(), nullptr,
					&program.fragment);
				validProgram = SUCCEEDED(result);
				if (!validProgram)
					logger::error("A shader failed to compile.");
			}
		}

		~Shader() noexcept
		{
			if (validProgram) {
				switch (stage) {
				case PipelineStage::Vertex:
					program.vertex->Release();
					break;
				case PipelineStage::Fragment:
					program.fragment->Release();
					break;
				}
			}
			validProgram = false;
		}

		Shader(const Shader&) = delete;
		Shader(Shader&&) noexcept = delete;
		Shader& operator=(const Shader&) = delete;
		Shader& operator=(Shader&&) noexcept = delete;

		// Use the shader for draw operations
		void Use() noexcept
		{
			if (stage == PipelineStage::Vertex) {
				context.context->VSSetShader(program.vertex, nullptr, 0);
			} else {
				context.context->PSSetShader(program.fragment, nullptr, 0);
			}
		}

		// Returns true if the shader is valid
		bool IsValid() const noexcept { return validProgram && validBinary; }

	private:
		D3DContext context;
		winrt::com_ptr<ID3DBlob> binary;
		PipelineStage stage;
		bool validBinary = false;
		bool validProgram = false;

		// no com_ptr here - we is being bad
		union
		{
			ID3D11VertexShader* vertex;
			ID3D11PixelShader* fragment;
		} program;

		bool Compile(const std::string& source, const std::string& entryName, const std::string& version) noexcept
		{
			UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
			winrt::com_ptr<ID3DBlob> errorBlob;

			auto versionStr =
				stage == PipelineStage::Vertex ? std::string("vs_").append(version) : std::string("ps_").append(version);

			auto result = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, entryName.c_str(),
				versionStr.c_str(), compileFlags, 0, binary.put(), errorBlob.put());

			if (!SUCCEEDED(result)) {
				return false;
			}

			return true;
		}

		friend class VertexBuffer;
	};

	using IALayout = std::vector<D3D11_INPUT_ELEMENT_DESC>;
	struct VertexBufferCreateInfo
	{
		size_t elementSize = 0;
		size_t numElements = 0;
		D3D11_SUBRESOURCE_DATA* elementData = nullptr;
		D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		D3D11_USAGE bufferUsage = D3D11_USAGE_IMMUTABLE;
		uint32_t cpuAccessFlags = 0;
		std::shared_ptr<Shader> vertexProgram;
		IALayout iaLayout = {};
	};

	class VertexBuffer
	{
	public:
		explicit VertexBuffer(const VertexBufferCreateInfo& createInfo, D3DContext& ctx) noexcept :
			context(ctx), stride(createInfo.elementSize), topology(createInfo.topology), vertexCount(createInfo.numElements)
		{
			CreateBuffer(createInfo.elementSize * createInfo.numElements, createInfo.bufferUsage, createInfo.cpuAccessFlags,
				createInfo.elementData);
			CreateIALayout(createInfo.iaLayout, createInfo.vertexProgram);
		}
		~VertexBuffer() noexcept
		{
			context.context->IASetInputLayout(nullptr);
			context.context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);

			if (buffer)
				buffer = nullptr;

			if (inputLayout)
				inputLayout = nullptr;
		}
		VertexBuffer(const VertexBuffer&) = delete;
		VertexBuffer(VertexBuffer&&) noexcept = delete;
		VertexBuffer& operator=(const VertexBuffer&) = delete;
		VertexBuffer& operator=(VertexBuffer&&) noexcept = delete;

		// Bind the vertex buffer for drawing
		void Bind(uint32_t offset = 0) noexcept
		{
			const auto buf = buffer.get();
			context.context->IASetInputLayout(inputLayout.get());
			context.context->IASetVertexBuffers(0, 1, &buf, &stride, &offset);
			context.context->IASetPrimitiveTopology(topology);
		}
		// Draw the full contents of the buffer
		void Draw() noexcept { context.context->Draw(vertexCount, 0); }
		// Draw the given number of elements from the buffer
		void DrawCount(uint32_t num) noexcept
		{
			assert(num <= vertexCount);
			context.context->Draw(num, 0);
		}
		// Map the buffer to CPU memory
		D3D11_MAPPED_SUBRESOURCE& Map(D3D11_MAP mode) noexcept
		{
			const auto code = context.context->Map(buffer.get(), 0, mode, 0, &mappedBuffer);
			if (!SUCCEEDED(code))
				logger::critical("Failed to map a D3D vertex buffer.");
			return mappedBuffer;
		}
		// Unmap the buffer
		void Unmap() noexcept { context.context->Unmap(buffer.get(), 0); }
		// Create the input assembler layout
		void CreateIALayout(const IALayout& layout, const std::shared_ptr<Shader>& vertexProgram) noexcept
		{
			if (inputLayout)
				inputLayout = nullptr;

			const auto layoutCode = context.device->CreateInputLayout(layout.data(), layout.size(),
				vertexProgram->binary->GetBufferPointer(), vertexProgram->binary->GetBufferSize(), inputLayout.put());
			if (!SUCCEEDED(layoutCode))
				logger::critical("Failed to create input assembler layout.");
		}

	private:
		uint32_t stride;
		uint32_t vertexCount;
		D3D11_PRIMITIVE_TOPOLOGY topology;
		D3DContext context;
		winrt::com_ptr<ID3D11Buffer> buffer;
		winrt::com_ptr<ID3D11InputLayout> inputLayout;
		D3D11_MAPPED_SUBRESOURCE mappedBuffer;

		void CreateBuffer(size_t size, D3D11_USAGE usage, uint32_t cpuAccessFlags,
			const D3D11_SUBRESOURCE_DATA* initialData) noexcept
		{
			D3D11_BUFFER_DESC desc = {};
			desc.ByteWidth = static_cast<UINT>(size);
			desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_VERTEX_BUFFER;
			desc.Usage = usage;
			desc.CPUAccessFlags = cpuAccessFlags;

			const auto code = context.device->CreateBuffer(&desc, initialData, buffer.put());
			if (!SUCCEEDED(code))
				logger::critical("Failed to create D3D vertex buffer.");
		}
	};

	typedef struct CBufferCreateInfo
	{
		size_t size = 0;
		void* initialData = nullptr;
		D3D11_USAGE bufferUsage = D3D11_USAGE_DYNAMIC;
		uint32_t cpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
	} CBufferCreateInfo;

	class CBuffer
	{
	public:
		CBuffer(CBufferCreateInfo& info, D3DContext& ctx)
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = info.size;
			desc.Usage = info.bufferUsage;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = info.cpuAccessFlags;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA init;
			init.pSysMem = info.initialData;
			init.SysMemPitch = 0;
			init.SysMemSlicePitch = 0;

			size = info.size;
			usage = info.bufferUsage;

			if (!SUCCEEDED(ctx.device->CreateBuffer(&desc, &init, buffer.put())))
				logger::critical("Failed to create D3D cbuffer.");
		}

		CBuffer(const CBuffer&) = delete;
		CBuffer(CBuffer&&) noexcept = delete;
		CBuffer& operator=(const CBuffer&) = delete;
		CBuffer& operator=(CBuffer&&) noexcept = delete;

		// Bind the constant buffer to a pipeline stage at the given location
		void Bind(PipelineStage stage, uint8_t loc, D3DContext& ctx)
		{
			const auto buf = buffer.get();
			switch (stage) {
			case PipelineStage::Vertex:
				ctx.context->VSSetConstantBuffers(loc, 1, &buf);
				break;
			case PipelineStage::Fragment:
				ctx.context->PSSetConstantBuffers(loc, 1, &buf);
				break;
			}
		}

		// Update the contents of the buffer, starting at offset with length size
		void Update(const void* newData, size_t offset, size_t a_size, D3DContext& ctx)
		{
			D3D11_MAPPED_SUBRESOURCE mappedBuffer = {};
			if (!SUCCEEDED(ctx.context->Map(buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuffer)))
				logger::critical("Failed to map cbuffer resource.");

			auto start = reinterpret_cast<intptr_t>(mappedBuffer.pData) + offset;
			memcpy(reinterpret_cast<void*>(start), newData, a_size);

			ctx.context->Unmap(buffer.get(), 0);
		}
		// Get the size of the buffer memory
		size_t Size() const noexcept { return size; }
		// Get the buffer usage it was created with
		D3D11_USAGE Usage() const noexcept { return usage; }

	private:
		winrt::com_ptr<ID3D11Buffer> buffer;
		D3D11_USAGE usage;
		size_t size;
	};

	namespace Shaders
	{
		constexpr const auto VertexColorPassThruVS = R"(
struct VS_INPUT {
	float4 vPos : POS;
	float4 vColor : COL;
};

struct VS_OUTPUT {
	float4 vPos : SV_POSITION;
	float4 vColor : COLOR0;
};

cbuffer PerFrame : register(b1) {
	float4x4 matProjView;
};

VS_OUTPUT main(VS_INPUT input) {
	float4 pos = float4(input.vPos.xyz, 1.0f);
	pos = mul(matProjView, pos);

	VS_OUTPUT output;
	output.vPos = pos;
	output.vColor = input.vColor;
	return output;
}
		)";

		constexpr const auto VertexColorPassThruPS = R"(
struct PS_INPUT {
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
};

struct PS_OUTPUT {
	float4 color : SV_Target;
};

PS_OUTPUT main(PS_INPUT input) {
	PS_OUTPUT output;
	output.color = input.color;
	return output;
}
		)";

		constexpr const auto TextVS = R"(
struct VS_INPUT {
	float4 pos : POS;
	float2 uv : TEX;
	float4 col : COL;
};

struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 uv : TEX;
	float4 col : COLOR0;
};

cbuffer PerFrame : register(b1) {
	float4x4 matProjView;
};

VS_OUTPUT main(VS_INPUT input) {
	float4 pos = float4(input.pos.xyz, 1.0f);
	pos = mul(matProjView, pos);

	VS_OUTPUT output;
	output.pos = pos;
	output.uv = input.uv;
	output.col = input.col;
	return output;
}
	)";

		constexpr const auto TextPS = R"(
struct PS_INPUT {
	float4 pos : SV_POSITION;
	float2 uv : TEX;
	float4 col : COLOR0;
};

struct PS_OUTPUT {
	float4 color : SV_Target;
};

Texture2D<float4> fontTexture : register(t0);
SamplerState fontSampler : register(s0);

float median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}

PS_OUTPUT main(PS_INPUT input) {
	PS_OUTPUT output;
	
	float4 msdfSample = fontTexture.Sample(fontSampler, input.uv);
	float sigDist = median(msdfSample.r, msdfSample.g, msdfSample.b) - 0.5;
	
	// Smooth step with fixed width
	float opacity = smoothstep(-0.5, 0.5, sigDist);
	
	output.color = float4(input.col.rgb, input.col.a * opacity);
	return output;
}
)";
	}

	typedef struct Point
	{
		glm::vec4 pos;
		glm::vec4 col;

		explicit Point(glm::vec3 position, glm::vec4 color) : col(color) { pos = { position.x, position.y, position.z, 1.0f }; }
	} Point;

	typedef struct DrawnPoint
	{
		Point point;
		float duration;
		double timestamp;

		DrawnPoint(Point&& a_point, double a_timestamp, float a_duration) :
			point(a_point), timestamp(a_timestamp), duration(a_duration) {};
	} DrawnPoint;

	typedef struct Line
	{
		Point start;
		Point end;
		double duration;
		double timestamp;
		Line(Point&& a_start, Point&& a_end, double a_timestamp, float a_duration) :
			start(a_start), end(a_end), timestamp(a_timestamp), duration(a_duration) {};
	} Line;

	using LineList = std::vector<Line>;

	// Number of points we can submit in a single draw call
	constexpr size_t LineDrawPointBatchSize = 64;
	// Number of buffers to use
	constexpr size_t NumBuffers = 2;

	class LineDrawer
	{
	public:
		explicit LineDrawer(D3DContext& ctx) { CreateObjects(ctx); }
		~LineDrawer()
		{
			for (auto i = 0; i < NumBuffers; i++) vbo[i].reset();

			vs.reset();
			ps.reset();
		}
		LineDrawer(const LineDrawer&) = delete;
		LineDrawer(LineDrawer&&) noexcept = delete;
		LineDrawer& operator=(const LineDrawer&) = delete;
		LineDrawer& operator=(LineDrawer&&) noexcept = delete;

		// Submit a list of lines for drawing
		void Submit(const LineList& lines) noexcept
		{
			vs->Use();
			ps->Use();

			auto begin = lines.cbegin();
			auto end = lines.cend();
			uint32_t batchCount = 0;

			while (begin != end) {
				DrawBatch(
					// Flip flop buffers to avoid pipeline stalls, if possible
					batchCount % static_cast<uint32_t>(NumBuffers), begin, end);
				batchCount++;
			}
		}

	protected:
		std::shared_ptr<Shader> vs;
		std::shared_ptr<Shader> ps;

	private:
		std::array<std::unique_ptr<VertexBuffer>, NumBuffers> vbo;

		void CreateObjects(D3DContext& ctx)
		{
			ShaderCreateInfo vsCreateInfo(Shaders::VertexColorPassThruVS, PipelineStage::Vertex);
			vs = std::make_shared<Shader>(vsCreateInfo, ctx);

			ShaderCreateInfo psCreateInfo(Shaders::VertexColorPassThruPS, PipelineStage::Fragment);
			ps = std::make_shared<Shader>(psCreateInfo, ctx);

			VertexBufferCreateInfo vbInfo;
			vbInfo.elementSize = sizeof(Point);
			vbInfo.numElements = LineDrawPointBatchSize * 2;
			vbInfo.topology = D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			vbInfo.bufferUsage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
			vbInfo.cpuAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
			vbInfo.vertexProgram = vs;
			vbInfo.iaLayout.emplace_back(
				D3D11_INPUT_ELEMENT_DESC{ "POS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
			vbInfo.iaLayout.emplace_back(D3D11_INPUT_ELEMENT_DESC{ "COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
				D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 });

			for (auto i = 0; i < NumBuffers; i++) vbo[i] = std::move(std::make_unique<VertexBuffer>(vbInfo, ctx));
		}
		void DrawBatch(uint32_t bufferIndex, LineList::const_iterator& begin, LineList::const_iterator& end)
		{
			uint32_t batchSize = 0;
			uint32_t index = 0;
			auto buf = reinterpret_cast<glm::vec4*>(vbo[bufferIndex]->Map(D3D11_MAP::D3D11_MAP_WRITE_DISCARD).pData);

			while (begin != end) {
				RE::NiPoint3 a_start{ begin->start.pos.x, begin->start.pos.y, begin->start.pos.z };
				glm::vec3 start{ a_start.x, a_start.y, a_start.z };
				RE::NiCamera::WorldPtToScreenPt3((float(*)[4])g_worldToCamMatrix, *g_viewPort, a_start, start.x, start.y, start.z,
					1e-5f);
				start -= 0.5f;
				start *= 2.f;

				RE::NiPoint3 a_end{ begin->end.pos.x, begin->end.pos.y, begin->end.pos.z };
				glm::vec3 _end{ a_end.x, a_end.y, a_end.z };
				RE::NiCamera::WorldPtToScreenPt3((float(*)[4])g_worldToCamMatrix, *g_viewPort, a_end, _end.x, _end.y, _end.z,
					1e-5f);
				_end -= 0.5f;
				_end *= 2.f;
				if (_end.z < 0 && start.z < 0) {
					begin++;
					continue;
				}

				glm::vec4 f_start{ start.x, start.y, start.z, 1.0f };
				glm::vec4 f_end{ _end.x, _end.y, _end.z, 1.0f };

				buf[index] = f_start;
				buf[index + 1] = begin->start.col;
				buf[index + 2] = f_end;
				buf[index + 3] = begin->end.col;

				begin++;
				batchSize++;
				index += 4;

				if (batchSize >= LineDrawPointBatchSize)
					break;
			}

			vbo[bufferIndex]->Unmap();
			vbo[bufferIndex]->Bind();
			vbo[bufferIndex]->DrawCount(batchSize * 2);
		}
	};

	struct TextureCreateInfo
	{
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t mipLevels = 1;
		uint32_t arraySize = 1;
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
		uint32_t bindFlags = D3D11_BIND_SHADER_RESOURCE;
		uint32_t cpuAccessFlags = 0;
		const void* initialData = nullptr;
	};

	static void ok_or_abort(HRESULT hr, const char* msg)
	{
		if (FAILED(hr)) {
			logger::critical("Win Error: %s", msg);
			std::abort();
		}
	}

	class Texture
	{
	public:
		Texture(const TextureCreateInfo& createInfo, D3DContext& ctx)
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = createInfo.width;
			desc.Height = createInfo.height;
			desc.MipLevels = createInfo.mipLevels;
			desc.ArraySize = createInfo.arraySize;
			desc.Format = createInfo.format;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = createInfo.usage;
			desc.BindFlags = createInfo.bindFlags;
			desc.CPUAccessFlags = createInfo.cpuAccessFlags;
			desc.MiscFlags = 0;

			D3D11_SUBRESOURCE_DATA initData = {};
			initData.pSysMem = createInfo.initialData;
			initData.SysMemPitch = createInfo.width * 4;  // For RGBA8 (4 bytes per pixel)

			HRESULT hr = ctx.GetDevice()->CreateTexture2D(&desc, createInfo.initialData ? &initData : nullptr, &texture);
			ok_or_abort(hr, "Failed to create texture");

			// Create shader resource view
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = desc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = desc.MipLevels;
			srvDesc.Texture2D.MostDetailedMip = 0;

			hr = ctx.GetDevice()->CreateShaderResourceView(texture, &srvDesc, &srv);
			ok_or_abort(hr, "Failed to create shader resource view");
		}

		~Texture()
		{
			if (srv)
				srv->Release();
			if (texture)
				texture->Release();
		}

		void Bind(uint32_t slot, PipelineStage stage, D3DContext& ctx)
		{
			ID3D11DeviceContext* d3dContext = ctx.GetContext();

			if (stage == PipelineStage::Vertex) {
				d3dContext->VSSetShaderResources(slot, 1, &srv);
			} else if (stage == PipelineStage::Fragment) {
				d3dContext->PSSetShaderResources(slot, 1, &srv);
			}
		}

	private:
		ID3D11Texture2D* texture = nullptr;
		ID3D11ShaderResourceView* srv = nullptr;
	};

	struct SamplerStateCreateInfo
	{
		D3D11_FILTER filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		D3D11_TEXTURE_ADDRESS_MODE addressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		D3D11_TEXTURE_ADDRESS_MODE addressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		D3D11_TEXTURE_ADDRESS_MODE addressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	};

	class SamplerState
	{
	public:
		SamplerState(const SamplerStateCreateInfo& createInfo, D3DContext& ctx)
		{
			D3D11_SAMPLER_DESC desc = {};
			desc.Filter = static_cast<D3D11_FILTER>(createInfo.filter);
			desc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(createInfo.addressU);
			desc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(createInfo.addressV);
			desc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(createInfo.addressW);
			desc.MipLODBias = 0.0f;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
			desc.BorderColor[0] = 1.0f;
			desc.BorderColor[1] = 1.0f;
			desc.BorderColor[2] = 1.0f;
			desc.BorderColor[3] = 1.0f;
			desc.MinLOD = 0;
			desc.MaxLOD = D3D11_FLOAT32_MAX;

			HRESULT hr = ctx.GetDevice()->CreateSamplerState(&desc, &sampler);
			ok_or_abort(hr, "Failed to create sampler state");
		}

		~SamplerState()
		{
			if (sampler)
				sampler->Release();
		}

		void Bind(uint32_t slot, PipelineStage stage, D3DContext& ctx) const
		{
			ID3D11DeviceContext* d3dContext = ctx.GetContext();

			if (stage == PipelineStage::Vertex) {
				d3dContext->VSSetSamplers(slot, 1, &sampler);
			} else if (stage == PipelineStage::Fragment) {
				d3dContext->PSSetSamplers(slot, 1, &sampler);
			}
		}

	private:
		ID3D11SamplerState* sampler = nullptr;
	};

	struct TextElement
	{
		std::string text;
		glm::vec3 position;
		glm::vec4 color;
		float fontSize;
		double timestamp;
		double duration;
		glm::vec3 normal;
		bool useNormal;
		bool drawOnTop;

		TextElement() = default;

		TextElement(const std::string& t, const RE::NiPoint3& pos, glm::vec4 col, float size, double ts, float dur,
			const RE::NiPoint3& norm = {}, bool onTop = false, bool usenormal = false) :
			text(t), position(pos.x, pos.y, pos.z), color(std::move(col)), fontSize(size), timestamp(ts), duration(dur),
			normal(norm.x, norm.y, norm.z), drawOnTop(onTop), useNormal(usenormal)
		{}
	};

	class TextRenderer
	{
	public:
		explicit TextRenderer(D3DContext& ctx)
		{
			CreateObjects(ctx);
			BuildCharacterMap();
		}
		~TextRenderer()
		{
			for (auto i = 0; i < NumBuffers; i++) vbo[i].reset();
			vs.reset();
			ps.reset();
			texture.reset();
			sampler.reset();
		}
		TextRenderer(const TextRenderer&) = delete;
		TextRenderer(TextRenderer&&) noexcept = delete;
		TextRenderer& operator=(const TextRenderer&) = delete;
		TextRenderer& operator=(TextRenderer&&) noexcept = delete;

		void Submit(const std::vector<TextElement>& texts, D3DContext& ctx, const glm::mat4& matProjView) noexcept
		{
			vs->Use();
			ps->Use();
			texture->Bind(0, PipelineStage::Fragment, ctx);
			sampler->Bind(0, PipelineStage::Fragment, ctx);

			auto begin = texts.cbegin();
			auto end = texts.cend();
			uint32_t batchCount = 0;

			while (begin != end) {
				DrawBatch(batchCount % static_cast<uint32_t>(NumBuffers), begin, end, matProjView);
				batchCount++;
			}
		}

		static constexpr uint32_t CharsPerBatch = 100;

	protected:
		std::shared_ptr<Shader> vs;
		std::shared_ptr<Shader> ps;
		std::shared_ptr<Texture> texture;
		std::shared_ptr<SamplerState> sampler;

	private:
		struct TextVertex
		{
			glm::vec4 pos;
			glm::vec2 uv;
			glm::vec4 color;
		};
		static_assert(sizeof(TextVertex) == 40);

		static constexpr uint32_t VerticesPerChar = 6;
		static constexpr uint32_t VerticesPerBatch = CharsPerBatch * VerticesPerChar;
		static constexpr uint32_t NumBuffers = 3;
		std::array<std::unique_ptr<VertexBuffer>, NumBuffers> vbo;

		struct GlyphInfo
		{
			float u, v, u2, v2;  // UV coordinates (u,v = top-left, u2,v2 = bottom-right)
			float advance;
			float planeLeft, planeBottom, planeRight, planeTop;
		};
		std::unordered_map<wchar_t, GlyphInfo> charToGlyph;

		void BuildCharacterMap()
		{
			charToGlyph = {
				{ ' ', { 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.3f, 0.000000f, 0.000000f, 0.000000f, 0.000000f } },
				{ '%', { 0.893555f, 0.000977f, 0.991211f, 0.098633f, 0.9f, 0.040861f, 0.056281f, 0.844881f, -0.747739f } },
				{ '$', { 0.670898f, 0.000977f, 0.733398f, 0.114258f, 0.6f, 0.015175f, 0.120603f, 0.529747f, -0.812060f } },
				{ 'Д', { 0.735352f, 0.000977f, 0.817383f, 0.114258f, 0.7f, -0.016644f, 0.184925f, 0.658733f, -0.747739f } },
				{ 'Й', { 0.819336f, 0.000977f, 0.891602f, 0.114258f, 0.7f, 0.062132f, 0.024121f, 0.657107f, -0.908543f } },
				{ '_', { 0.893555f, 0.100586f, 0.969727f, 0.114258f, 0.6f, -0.037445f, 0.217085f, 0.589691f, 0.104523f } },
				{ '[', { 0.319336f, 0.000977f, 0.348633f, 0.118164f, 0.3f, 0.044192f, 0.217085f, 0.285398f, -0.747739f } },
				{ ']', { 0.350586f, 0.000977f, 0.379883f, 0.118164f, 0.3f, -0.004636f, 0.217085f, 0.236570f, -0.747739f } },
				{ 'Ц', { 0.381836f, 0.000977f, 0.463867f, 0.118164f, 0.7f, 0.054157f, 0.217085f, 0.729534f, -0.747739f } },
				{ 'Щ', { 0.465820f, 0.000977f, 0.571289f, 0.118164f, 0.9f, 0.056796f, 0.217085f, 0.925138f, -0.747739f } },
				{ 'ф', { 0.573242f, 0.000977f, 0.668945f, 0.118164f, 0.8f, 0.017407f, 0.217085f, 0.805347f, -0.747739f } },
				{ '(', { 0.000977f, 0.000977f, 0.034180f, 0.120117f, 0.3f, 0.042028f, 0.233166f, 0.315394f, -0.747739f } },
				{ ')', { 0.036133f, 0.000977f, 0.069336f, 0.120117f, 0.3f, 0.042028f, 0.233166f, 0.315394f, -0.747739f } },
				{ '@', { 0.071289f, 0.000977f, 0.188477f, 0.120117f, 1.0f, 0.034190f, 0.233166f, 0.999014f, -0.747739f } },
				{ 'j', { 0.190430f, 0.000977f, 0.219727f, 0.120117f, 0.2f, -0.066892f, 0.233166f, 0.174314f, -0.747739f } },
				{ '{', { 0.221680f, 0.000977f, 0.260742f, 0.120117f, 0.3f, 0.008385f, 0.233166f, 0.329993f, -0.747739f } },
				{ '|', { 0.262695f, 0.000977f, 0.276367f, 0.120117f, 0.3f, 0.073846f, 0.233166f, 0.186408f, -0.747739f } },
				{ '}', { 0.278320f, 0.000977f, 0.317383f, 0.120117f, 0.3f, 0.003503f, 0.233166f, 0.325111f, -0.747739f } },
				{ '#', { 0.159180f, 0.122070f, 0.229492f, 0.217773f, 0.6f, -0.012592f, 0.040201f, 0.566303f, -0.747739f } },
				{ '&', { 0.231445f, 0.122070f, 0.309570f, 0.217773f, 0.7f, 0.021898f, 0.040201f, 0.665114f, -0.747739f } },
				{ '/', { 0.311523f, 0.122070f, 0.350586f, 0.217773f, 0.3f, -0.021888f, 0.040201f, 0.299720f, -0.747739f } },
				{ '0', { 0.352539f, 0.122070f, 0.415039f, 0.217773f, 0.6f, 0.017616f, 0.040201f, 0.532189f, -0.747739f } },
				{ '3', { 0.416992f, 0.122070f, 0.479492f, 0.217773f, 0.6f, 0.019081f, 0.040201f, 0.533654f, -0.747739f } },
				{ '6', { 0.481445f, 0.122070f, 0.543945f, 0.217773f, 0.6f, 0.016639f, 0.040201f, 0.531212f, -0.747739f } },
				{ '8', { 0.545898f, 0.122070f, 0.608398f, 0.217773f, 0.6f, 0.019081f, 0.040201f, 0.533654f, -0.747739f } },
				{ '9', { 0.610352f, 0.122070f, 0.672852f, 0.217773f, 0.6f, 0.019569f, 0.040201f, 0.534142f, -0.747739f } },
				{ 'C', { 0.674805f, 0.122070f, 0.756836f, 0.217773f, 0.7f, 0.028522f, 0.040201f, 0.703899f, -0.747739f } },
				{ 'G', { 0.758789f, 0.122070f, 0.844727f, 0.217773f, 0.8f, 0.030508f, 0.040201f, 0.738046f, -0.747739f } },
				{ 'J', { 0.846680f, 0.122070f, 0.899414f, 0.217773f, 0.5f, 0.008464f, 0.040201f, 0.442635f, -0.747739f } },
				{ 'O', { 0.901367f, 0.122070f, 0.989258f, 0.217773f, 0.8f, 0.028816f, 0.040201f, 0.752434f, -0.747739f } },
				{ 'б', { 0.092773f, 0.122070f, 0.157227f, 0.219727f, 0.6f, 0.023492f, 0.040201f, 0.554145f, -0.763819f } },
				{ 'Q', { 0.000977f, 0.122070f, 0.090820f, 0.221680f, 0.8f, 0.022241f, 0.072362f, 0.761939f, -0.747739f } },
				{ 'я', { 0.938477f, 0.223633f, 0.999023f, 0.291992f, 0.5f, -0.004617f, 0.024121f, 0.493875f, -0.538693f } },
				{ '.', { 0.967773f, 0.293945f, 0.985352f, 0.311523f, 0.3f, 0.068507f, 0.024121f, 0.213231f, -0.120603f } },
				{ '`', { 0.938477f, 0.293945f, 0.965820f, 0.317383f, 0.3f, 0.022691f, -0.554774f, 0.247817f, -0.747739f } },
				{ 'S', { 0.000977f, 0.223633f, 0.075195f, 0.319336f, 0.7f, 0.024306f, 0.040201f, 0.635362f, -0.747739f } },
				{ 'U', { 0.077148f, 0.223633f, 0.151367f, 0.319336f, 0.7f, 0.054580f, 0.040201f, 0.665635f, -0.747739f } },
				{ 'b', { 0.153320f, 0.223633f, 0.211914f, 0.319336f, 0.6f, 0.049077f, 0.040201f, 0.531489f, -0.747739f } },
				{ 'Ю', { 0.213867f, 0.223633f, 0.325195f, 0.319336f, 1.0f, 0.061484f, 0.040201f, 0.978067f, -0.747739f } },
				{ 'Э', { 0.327148f, 0.223633f, 0.409180f, 0.319336f, 0.7f, 0.012817f, 0.040201f, 0.688193f, -0.747739f } },
				{ 'd', { 0.411133f, 0.223633f, 0.469727f, 0.319336f, 0.6f, 0.017827f, 0.040201f, 0.500239f, -0.747739f } },
				{ 'g', { 0.471680f, 0.223633f, 0.532227f, 0.319336f, 0.6f, 0.011496f, 0.233166f, 0.509988f, -0.554774f } },
				{ 'З', { 0.534180f, 0.223633f, 0.602539f, 0.319336f, 0.6f, 0.019374f, 0.040201f, 0.582188f, -0.747739f } },
				{ 'Л', { 0.604492f, 0.223633f, 0.678711f, 0.319336f, 0.7f, -0.012315f, 0.040201f, 0.598741f, -0.747739f } },
				{ 'О', { 0.680664f, 0.223633f, 0.768555f, 0.319336f, 0.8f, 0.028816f, 0.040201f, 0.752434f, -0.747739f } },
				{ 'С', { 0.770508f, 0.223633f, 0.852539f, 0.319336f, 0.7f, 0.028522f, 0.040201f, 0.703899f, -0.747739f } },
				{ 'У', { 0.854492f, 0.223633f, 0.936523f, 0.319336f, 0.6f, -0.019085f, 0.040201f, 0.656292f, -0.747739f } },
				{ '!', { 0.000977f, 0.321289f, 0.018555f, 0.415039f, 0.3f, 0.068019f, 0.024121f, 0.212743f, -0.747739f } },
				{ 'у', { 0.020508f, 0.321289f, 0.083008f, 0.415039f, 0.5f, -0.003624f, 0.233166f, 0.510949f, -0.538693f } },
				{ 'Р', { 0.084961f, 0.321289f, 0.155273f, 0.415039f, 0.7f, 0.060895f, 0.024121f, 0.639789f, -0.747739f } },
				{ 'П', { 0.157227f, 0.321289f, 0.231445f, 0.415039f, 0.7f, 0.054092f, 0.024121f, 0.665147f, -0.747739f } },
				{ 'Ч', { 0.233398f, 0.321289f, 0.303711f, 0.415039f, 0.7f, 0.025738f, 0.024121f, 0.604633f, -0.747739f } },
				{ 'Н', { 0.305664f, 0.321289f, 0.377930f, 0.415039f, 0.7f, 0.063352f, 0.024121f, 0.658327f, -0.747739f } },
				{ 'М', { 0.379883f, 0.321289f, 0.467773f, 0.415039f, 0.8f, 0.053962f, 0.024121f, 0.777581f, -0.747739f } },
				{ 'Ш', { 0.469727f, 0.321289f, 0.567383f, 0.415039f, 0.9f, 0.056486f, 0.024121f, 0.860506f, -0.747739f } },
				{ 'К', { 0.569336f, 0.321289f, 0.635742f, 0.415039f, 0.6f, 0.055491f, 0.024121f, 0.602224f, -0.747739f } },
				{ 'р', { 0.637695f, 0.321289f, 0.696289f, 0.415039f, 0.6f, 0.049810f, 0.217085f, 0.532222f, -0.554774f } },
				{ 'И', { 0.698242f, 0.321289f, 0.770508f, 0.415039f, 0.7f, 0.062132f, 0.024121f, 0.657107f, -0.747739f } },
				{ 'Ф', { 0.772461f, 0.321289f, 0.860352f, 0.415039f, 0.8f, 0.018074f, 0.024121f, 0.741692f, -0.747739f } },
				{ 'Ж', { 0.862305f, 0.321289f, 0.977539f, 0.415039f, 0.9f, -0.012946f, 0.024121f, 0.935798f, -0.747739f } },
				{ 'I', { 0.979492f, 0.321289f, 0.995117f, 0.415039f, 0.3f, 0.076303f, 0.024121f, 0.204947f, -0.747739f } },
				{ 'Е', { 0.000977f, 0.416992f, 0.071289f, 0.510742f, 0.7f, 0.056744f, 0.024121f, 0.635639f, -0.747739f } },
				{ 'Г', { 0.073242f, 0.416992f, 0.133789f, 0.510742f, 0.5f, 0.060812f, 0.024121f, 0.559305f, -0.747739f } },
				{ 'В', { 0.135742f, 0.416992f, 0.206055f, 0.510742f, 0.7f, 0.054059f, 0.024121f, 0.632953f, -0.747739f } },
				{ 'Б', { 0.208008f, 0.416992f, 0.278320f, 0.510742f, 0.7f, 0.061871f, 0.024121f, 0.640766f, -0.747739f } },
				{ 'А', { 0.280273f, 0.416992f, 0.366211f, 0.510742f, 0.7f, -0.020273f, 0.024121f, 0.687265f, -0.747739f } },
				{ 'Т', { 0.368164f, 0.416992f, 0.442383f, 0.510742f, 0.6f, 0.001601f, 0.024121f, 0.612657f, -0.747739f } },
				{ '1', { 0.444336f, 0.416992f, 0.481445f, 0.510742f, 0.6f, 0.087959f, 0.024121f, 0.393486f, -0.747739f } },
				{ '2', { 0.483398f, 0.416992f, 0.545898f, 0.510742f, 0.6f, 0.009520f, 0.024121f, 0.524093f, -0.747739f } },
				{ '4', { 0.547852f, 0.416992f, 0.612305f, 0.510742f, 0.6f, -0.005073f, 0.024121f, 0.525581f, -0.747739f } },
				{ '5', { 0.614258f, 0.416992f, 0.676758f, 0.510742f, 0.6f, 0.021522f, 0.040201f, 0.536095f, -0.731658f } },
				{ 'y', { 0.678711f, 0.416992f, 0.741211f, 0.510742f, 0.5f, -0.003624f, 0.233166f, 0.510949f, -0.538693f } },
				{ '?', { 0.743164f, 0.416992f, 0.803711f, 0.510742f, 0.6f, 0.025656f, 0.024121f, 0.524149f, -0.747739f } },
				{ 'A', { 0.805664f, 0.416992f, 0.891602f, 0.510742f, 0.7f, -0.020273f, 0.024121f, 0.687265f, -0.747739f } },
				{ 'B', { 0.893555f, 0.416992f, 0.963867f, 0.510742f, 0.7f, 0.054059f, 0.024121f, 0.632953f, -0.747739f } },
				{ 'i', { 0.965820f, 0.416992f, 0.981445f, 0.510742f, 0.2f, 0.046030f, 0.024121f, 0.174673f, -0.747739f } },
				{ 'l', { 0.983398f, 0.416992f, 0.999023f, 0.510742f, 0.2f, 0.043589f, 0.024121f, 0.172232f, -0.747739f } },
				{ 'х', { 0.934570f, 0.512695f, 0.999023f, 0.581055f, 0.5f, -0.015327f, 0.024121f, 0.515327f, -0.538693f } },
				{ '-', { 0.934570f, 0.583008f, 0.971680f, 0.600586f, 0.3f, 0.013984f, -0.184925f, 0.319512f, -0.329648f } },
				{ 'D', { 0.000977f, 0.512695f, 0.077148f, 0.606445f, 0.7f, 0.059479f, 0.024121f, 0.686615f, -0.747739f } },
				{ 'E', { 0.079102f, 0.512695f, 0.149414f, 0.606445f, 0.7f, 0.056744f, 0.024121f, 0.635639f, -0.747739f } },
				{ 'q', { 0.151367f, 0.512695f, 0.209961f, 0.606445f, 0.6f, 0.018560f, 0.217085f, 0.500972f, -0.554774f } },
				{ 'p', { 0.211914f, 0.512695f, 0.270508f, 0.606445f, 0.6f, 0.049810f, 0.217085f, 0.532222f, -0.554774f } },
				{ 'F', { 0.272461f, 0.512695f, 0.336914f, 0.606445f, 0.6f, 0.058160f, 0.024121f, 0.588813f, -0.747739f } },
				{ 'H', { 0.338867f, 0.512695f, 0.411133f, 0.606445f, 0.7f, 0.063352f, 0.024121f, 0.658327f, -0.747739f } },
				{ 'K', { 0.413086f, 0.512695f, 0.489258f, 0.606445f, 0.7f, 0.055573f, 0.024121f, 0.682708f, -0.747739f } },
				{ 'L', { 0.491211f, 0.512695f, 0.549805f, 0.606445f, 0.6f, 0.055669f, 0.024121f, 0.538081f, -0.747739f } },
				{ 'k', { 0.551758f, 0.512695f, 0.608398f, 0.606445f, 0.5f, 0.048084f, 0.024121f, 0.514416f, -0.747739f } },
				{ 'M', { 0.610352f, 0.512695f, 0.698242f, 0.606445f, 0.8f, 0.053962f, 0.024121f, 0.777581f, -0.747739f } },
				{ 'N', { 0.700195f, 0.512695f, 0.774414f, 0.606445f, 0.7f, 0.052627f, 0.024121f, 0.663682f, -0.747739f } },
				{ 'h', { 0.776367f, 0.512695f, 0.833008f, 0.606445f, 0.6f, 0.043934f, 0.024121f, 0.510265f, -0.747739f } },
				{ 'Ъ', { 0.834961f, 0.512695f, 0.932617f, 0.606445f, 0.8f, -0.023348f, 0.024121f, 0.780672f, -0.747739f } },
				{ 'з', { 0.946289f, 0.608398f, 0.999023f, 0.680664f, 0.5f, 0.006547f, 0.040201f, 0.440718f, -0.554774f } },
				{ 'Ы', { 0.000977f, 0.608398f, 0.092773f, 0.702148f, 0.9f, 0.064249f, 0.024121f, 0.820028f, -0.747739f } },
				{ 'f', { 0.094727f, 0.608398f, 0.135742f, 0.702148f, 0.3f, -0.007956f, 0.024121f, 0.329733f, -0.747739f } },
				{ 'Ь', { 0.137695f, 0.608398f, 0.208008f, 0.702148f, 0.7f, 0.060895f, 0.024121f, 0.639789f, -0.747739f } },
				{ 'P', { 0.209961f, 0.608398f, 0.280273f, 0.702148f, 0.7f, 0.060895f, 0.024121f, 0.639789f, -0.747739f } },
				{ 'Х', { 0.282227f, 0.608398f, 0.366211f, 0.702148f, 0.7f, -0.013209f, 0.024121f, 0.678248f, -0.747739f } },
				{ 'R', { 0.368164f, 0.608398f, 0.450195f, 0.702148f, 0.7f, 0.056355f, 0.024121f, 0.731731f, -0.747739f } },
				{ 'T', { 0.452148f, 0.608398f, 0.526367f, 0.702148f, 0.6f, 0.001601f, 0.024121f, 0.612657f, -0.747739f } },
				{ 'Я', { 0.528320f, 0.608398f, 0.610352f, 0.702148f, 0.7f, -0.009563f, 0.024121f, 0.665813f, -0.747739f } },
				{ 'V', { 0.612305f, 0.608398f, 0.696289f, 0.702148f, 0.7f, -0.013942f, 0.024121f, 0.677516f, -0.747739f } },
				{ 'Z', { 0.698242f, 0.608398f, 0.772461f, 0.702148f, 0.6f, -0.002549f, 0.024121f, 0.608506f, -0.747739f } },
				{ 'Y', { 0.774414f, 0.608398f, 0.858398f, 0.702148f, 0.7f, -0.014674f, 0.024121f, 0.676783f, -0.747739f } },
				{ 'X', { 0.860352f, 0.608398f, 0.944336f, 0.702148f, 0.7f, -0.013209f, 0.024121f, 0.678248f, -0.747739f } },
				{ 'о', { 0.530273f, 0.704102f, 0.594727f, 0.776367f, 0.6f, 0.010796f, 0.040201f, 0.541450f, -0.554774f } },
				{ 'с', { 0.596680f, 0.704102f, 0.657227f, 0.776367f, 0.5f, 0.015646f, 0.040201f, 0.514139f, -0.554774f } },
				{ 'e', { 0.659180f, 0.704102f, 0.721680f, 0.776367f, 0.6f, 0.018348f, 0.040201f, 0.532921f, -0.554774f } },
				{ 's', { 0.723633f, 0.704102f, 0.780273f, 0.776367f, 0.5f, 0.012928f, 0.040201f, 0.479260f, -0.554774f } },
				{ 'ю', { 0.782227f, 0.704102f, 0.864258f, 0.776367f, 0.8f, 0.050495f, 0.040201f, 0.725872f, -0.554774f } },
				{ 'c', { 0.866211f, 0.704102f, 0.926758f, 0.776367f, 0.5f, 0.015646f, 0.040201f, 0.514139f, -0.554774f } },
				{ 'а', { 0.928711f, 0.704102f, 0.991211f, 0.776367f, 0.6f, 0.017616f, 0.040201f, 0.532189f, -0.554774f } },
				{ 'ц', { 0.278320f, 0.704102f, 0.340820f, 0.790039f, 0.6f, 0.047889f, 0.168844f, 0.562462f, -0.538693f } },
				{ 'щ', { 0.342773f, 0.704102f, 0.434570f, 0.790039f, 0.8f, 0.052042f, 0.168844f, 0.807821f, -0.538693f } },
				{ ';', { 0.436523f, 0.704102f, 0.454102f, 0.790039f, 0.3f, 0.063625f, 0.168844f, 0.208348f, -0.538693f } },
				{ 'д', { 0.456055f, 0.704102f, 0.528320f, 0.790039f, 0.6f, -0.021120f, 0.168844f, 0.573855f, -0.538693f } },
				{ 'й', { 0.120117f, 0.704102f, 0.176758f, 0.795898f, 0.6f, 0.045887f, 0.024121f, 0.512219f, -0.731658f } },
				{ '7', { 0.178711f, 0.704102f, 0.239258f, 0.795898f, 0.6f, 0.029807f, 0.024121f, 0.528299f, -0.731658f } },
				{ 't', { 0.241211f, 0.704102f, 0.276367f, 0.795898f, 0.3f, -0.000681f, 0.024121f, 0.288767f, -0.731658f } },
				{ 'W', { 0.000977f, 0.704102f, 0.118164f, 0.797852f, 0.9f, -0.010000f, 0.024121f, 0.954824f, -0.747739f } },
				{ 'м', { 0.506836f, 0.799805f, 0.579102f, 0.868164f, 0.7f, 0.046263f, 0.024121f, 0.641237f, -0.538693f } },
				{ 'ь', { 0.581055f, 0.799805f, 0.637695f, 0.868164f, 0.5f, 0.044178f, 0.024121f, 0.510510f, -0.538693f } },
				{ 'в', { 0.639648f, 0.799805f, 0.696289f, 0.868164f, 0.5f, 0.046131f, 0.024121f, 0.512463f, -0.538693f } },
				{ 'л', { 0.698242f, 0.799805f, 0.764648f, 0.868164f, 0.6f, -0.008963f, 0.024121f, 0.537771f, -0.538693f } },
				{ 'н', { 0.766602f, 0.799805f, 0.823242f, 0.868164f, 0.6f, 0.042957f, 0.024121f, 0.509289f, -0.538693f } },
				{ ':', { 0.825195f, 0.799805f, 0.842773f, 0.868164f, 0.3f, 0.068019f, 0.024121f, 0.212743f, -0.538693f } },
				{ 'w', { 0.844727f, 0.799805f, 0.936523f, 0.868164f, 0.7f, -0.019247f, 0.024121f, 0.736532f, -0.538693f } },
				{ 'z', { 0.938477f, 0.799805f, 0.999023f, 0.868164f, 0.5f, -0.000223f, 0.024121f, 0.498270f, -0.538693f } },
				{ 'm', { 0.256836f, 0.799805f, 0.346680f, 0.870117f, 0.8f, 0.047387f, 0.024121f, 0.787086f, -0.554774f } },
				{ 'u', { 0.348633f, 0.799805f, 0.405273f, 0.870117f, 0.6f, 0.041004f, 0.040201f, 0.507336f, -0.538693f } },
				{ 'n', { 0.407227f, 0.799805f, 0.463867f, 0.870117f, 0.6f, 0.043445f, 0.024121f, 0.509777f, -0.554774f } },
				{ 'r', { 0.465820f, 0.799805f, 0.504883f, 0.870117f, 0.3f, 0.045007f, 0.024121f, 0.366615f, -0.554774f } },
				{ 'е', { 0.000977f, 0.799805f, 0.063477f, 0.872070f, 0.6f, 0.018348f, 0.040201f, 0.532921f, -0.554774f } },
				{ 'э', { 0.065430f, 0.799805f, 0.124023f, 0.872070f, 0.5f, 0.004711f, 0.040201f, 0.487123f, -0.554774f } },
				{ 'o', { 0.125977f, 0.799805f, 0.190430f, 0.872070f, 0.6f, 0.010796f, 0.040201f, 0.541450f, -0.554774f } },
				{ 'a', { 0.192383f, 0.799805f, 0.254883f, 0.872070f, 0.6f, 0.017616f, 0.040201f, 0.532189f, -0.554774f } },
				{ '>', { 0.790039f, 0.874023f, 0.852539f, 0.938477f, 0.6f, 0.034462f, -0.088442f, 0.549034f, -0.619095f } },
				{ '+', { 0.854492f, 0.874023f, 0.916992f, 0.938477f, 0.6f, 0.034706f, -0.088442f, 0.549279f, -0.619095f } },
				{ '<', { 0.918945f, 0.874023f, 0.981445f, 0.938477f, 0.6f, 0.034462f, -0.088442f, 0.549034f, -0.619095f } },
				{ 'ж', { 0.000977f, 0.874023f, 0.086914f, 0.942383f, 0.7f, -0.019052f, 0.024121f, 0.688486f, -0.538693f } },
				{ 'ш', { 0.088867f, 0.874023f, 0.174805f, 0.942383f, 0.8f, 0.047110f, 0.024121f, 0.754648f, -0.538693f } },
				{ 'x', { 0.176758f, 0.874023f, 0.241211f, 0.942383f, 0.5f, -0.015327f, 0.024121f, 0.515327f, -0.538693f } },
				{ 'г', { 0.243164f, 0.874023f, 0.284180f, 0.942383f, 0.4f, 0.046732f, 0.024121f, 0.384420f, -0.538693f } },
				{ 'и', { 0.286133f, 0.874023f, 0.342773f, 0.942383f, 0.6f, 0.045887f, 0.024121f, 0.512219f, -0.538693f } },
				{ 'ы', { 0.344727f, 0.874023f, 0.420898f, 0.942383f, 0.7f, 0.044098f, 0.024121f, 0.671234f, -0.538693f } },
				{ 'ъ', { 0.422852f, 0.874023f, 0.497070f, 0.942383f, 0.6f, -0.001084f, 0.024121f, 0.609971f, -0.538693f } },
				{ 'ч', { 0.499023f, 0.874023f, 0.555664f, 0.942383f, 0.5f, 0.010975f, 0.024121f, 0.477306f, -0.538693f } },
				{ 'к', { 0.557617f, 0.874023f, 0.608398f, 0.942383f, 0.4f, 0.046326f, 0.024121f, 0.464416f, -0.538693f } },
				{ 'п', { 0.610352f, 0.874023f, 0.665039f, 0.942383f, 0.5f, 0.045870f, 0.024121f, 0.496122f, -0.538693f } },
				{ 'v', { 0.666992f, 0.874023f, 0.729492f, 0.942383f, 0.5f, -0.006798f, 0.024121f, 0.507775f, -0.538693f } },
				{ 'т', { 0.731445f, 0.874023f, 0.788086f, 0.942383f, 0.5f, -0.003918f, 0.024121f, 0.462414f, -0.538693f } },
				{ '~', { 0.247070f, 0.944336f, 0.313477f, 0.969727f, 0.6f, 0.018869f, -0.249246f, 0.565603f, -0.458291f } },
				{ ',', { 0.227539f, 0.944336f, 0.245117f, 0.979492f, 0.3f, 0.063625f, 0.168844f, 0.208348f, -0.120603f } },
				{ '\"', { 0.168945f, 0.944336f, 0.206055f, 0.981445f, 0.4f, 0.024238f, -0.442211f, 0.329766f, -0.747739f } },
				{ '\'', { 0.208008f, 0.944336f, 0.225586f, 0.981445f, 0.2f, 0.021632f, -0.442211f, 0.166356f, -0.747739f } },
				{ '=', { 0.104492f, 0.944336f, 0.166992f, 0.985352f, 0.6f, 0.034706f, -0.184925f, 0.549279f, -0.522613f } },
				{ '*', { 0.057617f, 0.944336f, 0.102539f, 0.987305f, 0.4f, 0.007702f, -0.393970f, 0.377552f, -0.747739f } },
				{ '^', { 0.000977f, 0.944336f, 0.055664f, 0.997070f, 0.5f, 0.009494f, -0.313568f, 0.459745f, -0.747739f } },
			};
		}

		static bool ReadRGBAFile(const std::string& filename, std::vector<uint8_t>& outData, uint32_t width, uint32_t height)
		{
			std::ifstream file(filename, std::ios::binary);
			if (!file.is_open()) {
				return false;
			}

			size_t expectedSize = static_cast<size_t>(width) * height * 4;

			outData.resize(expectedSize);
			file.read(reinterpret_cast<char*>(outData.data()), expectedSize);

			bool success = file.gcount() == int(expectedSize);
			file.close();
			return success;
		}

		static constexpr float atlasWidth = 512.0f;
		static constexpr float atlasHeight = 512.0f;

		void CreateObjects(D3DContext& ctx)
		{
			ShaderCreateInfo vsCreateInfo(Shaders::TextVS, PipelineStage::Vertex);
			vs = std::make_shared<Shader>(vsCreateInfo, ctx);

			ShaderCreateInfo psCreateInfo(Shaders::TextPS, PipelineStage::Fragment);
			ps = std::make_shared<Shader>(psCreateInfo, ctx);

			TextureCreateInfo texInfo;
			texInfo.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			texInfo.width = atlasWidth;
			texInfo.height = atlasHeight;
			texInfo.mipLevels = 1;
			texInfo.arraySize = 1;
			texInfo.usage = D3D11_USAGE_DEFAULT;
			texInfo.bindFlags = D3D11_BIND_SHADER_RESOURCE;

			std::string fontPath = "Data\\SKSE\\Plugins\\font_atlas.rgba";  // TODO
			std::vector<uint8_t> fontData;

			if (!ReadRGBAFile(fontPath, fontData, texInfo.width, texInfo.height)) {
				logger::critical("Failed to load font atlas: {}", fontPath);
				return;
			}

			texInfo.initialData = fontData.data();
			texture = std::make_shared<Texture>(texInfo, ctx);

			SamplerStateCreateInfo samplerInfo;
			samplerInfo.filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			samplerInfo.addressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerInfo.addressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerInfo.addressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampler = std::make_shared<SamplerState>(samplerInfo, ctx);

			VertexBufferCreateInfo vbInfo;
			vbInfo.elementSize = sizeof(TextVertex);
			vbInfo.numElements = VerticesPerBatch;
			vbInfo.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			vbInfo.bufferUsage = D3D11_USAGE_DYNAMIC;
			vbInfo.cpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
			vbInfo.vertexProgram = vs;

			vbInfo.iaLayout.emplace_back(
				D3D11_INPUT_ELEMENT_DESC{ "POS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
			vbInfo.iaLayout.emplace_back(D3D11_INPUT_ELEMENT_DESC{ "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
				D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 });
			vbInfo.iaLayout.emplace_back(D3D11_INPUT_ELEMENT_DESC{ "COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
				D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 });

			for (auto i = 0; i < NumBuffers; i++) {
				vbo[i] = std::make_unique<VertexBuffer>(vbInfo, ctx);
			}
		}

		void DrawBatch(uint32_t bufferIndex, std::vector<TextElement>::const_iterator& begin,
			std::vector<TextElement>::const_iterator& end, [[maybe_unused]] const glm::mat4& matProjView)
		{
			uint32_t batchSize = 0;
			uint32_t vertexIndex = 0;
			auto buf = reinterpret_cast<TextVertex*>(vbo[bufferIndex]->Map(D3D11_MAP_WRITE_DISCARD).pData);

			while (begin != end) {
				uint32_t verticesWillUsed = begin->text.length() * VerticesPerChar;
				if (vertexIndex + verticesWillUsed >= VerticesPerBatch) {
					break;
				}

				uint32_t processedCharacters = GenerateTextQuads(buf + vertexIndex, *begin, matProjView);

				begin++;
				batchSize++;
				vertexIndex += processedCharacters * VerticesPerChar;
			}

			vbo[bufferIndex]->Unmap();
			vbo[bufferIndex]->Bind();
			vbo[bufferIndex]->DrawCount(vertexIndex);
		}

		uint32_t GenerateTextQuads(TextVertex* vertices, const TextElement& text, [[maybe_unused]] const glm::mat4& matProjView)
		{
			RE::NiPoint3 position = { text.position.x, text.position.y, text.position.z };

			RE::NiPoint3 normal;
			if (text.useNormal) {
				normal = { text.normal.x, text.normal.y, text.normal.z };
			} else {
				normal = RE::PlayerCamera::GetSingleton()->pos - position;
			}
			normal *= -1;
			RE::NiPoint3 normal_dir = normal;
			normal_dir.Unitize();
			RE::NiPoint3 up_dir = { 0, 0, -1 };
			RE::NiPoint3 right_dir = up_dir.Cross(normal);
			float right_len = right_dir.Length();
			if (right_len < 0.001f) {
				right_dir = RE::NiPoint3(1, 0, 0);
			} else {
				right_dir = right_dir / right_len;
			}

			auto world2screen = [](const RE::NiPoint3& position) -> std::optional<RE::NiPoint3> {
				RE::NiPoint3 position_screen;
				if (!RE::NiCamera::WorldPtToScreenPt3((float(*)[4])g_worldToCamMatrix, *g_viewPort, position, position_screen.x,
						position_screen.y, position_screen.z, 1e-5f) ||
					position_screen.z < 0) {
					return std::nullopt;
				}
				position_screen -= RE::NiPoint3(0.5f, 0.5f, 0.5f);
				position_screen *= 2.f;
				return { position_screen };
			};

			uint32_t processedCharacters = 0;
			auto mb_position_screen = world2screen(position);
			if (!mb_position_screen)
				return processedCharacters;
			RE::NiPoint3 position_screen = *mb_position_screen;

			float scale = text.fontSize * 100;
			float uvBiasX = 0.5f / atlasWidth;
			float uvBiasY = 0.5f / atlasHeight;

			float totalWidth = 0.0f;
			for (size_t i = 0; i < text.text.length(); i++) {
				char c = text.text[i];
				auto it = charToGlyph.find(c);
				if (it == charToGlyph.end())
					continue;

				totalWidth += it->second.advance * scale;
			}

			RE::NiPoint3 current = position;
			current -= right_dir * (totalWidth * 0.5f);

			for (size_t i = 0; i < text.text.length(); i++) {
				char c = text.text[i];

				auto it = charToGlyph.find(c);
				if (it == charToGlyph.end())
					continue;

				const GlyphInfo& glyph = it->second;

				float uvLeft = glyph.u + uvBiasX * 6;
				float uvRight = glyph.u2 + uvBiasX * 6;
				float uvTop = glyph.v + uvBiasY;
				float uvBottom = glyph.v2 - uvBiasY;

				float planeWidth = glyph.planeRight - glyph.planeLeft;
				float len_offset_up = glyph.planeTop * scale;
				float len_offset_down = glyph.planeBottom * scale;
				float len_offset_right = planeWidth * scale;
				auto offset_up = up_dir * len_offset_up;
				auto offset_down = up_dir * len_offset_down;
				auto offset_right = right_dir * len_offset_right;
				RE::NiPoint3 world_left_top = current + offset_up;
				RE::NiPoint3 world_left_bottom = current + offset_down;
				RE::NiPoint3 world_right_top = current + offset_up + offset_right;
				RE::NiPoint3 world_right_bottom = current + offset_down + offset_right;

				auto mb_left_top = world2screen(world_left_top);
				auto mb_left_bottom = world2screen(world_left_bottom);
				auto mb_right_top = world2screen(world_right_top);
				auto mb_right_bottom = world2screen(world_right_bottom);

				if (mb_left_top && mb_left_bottom && mb_right_top && mb_right_bottom) {
					RE::NiPoint3 left_top = *mb_left_top;
					RE::NiPoint3 left_bottom = *mb_left_bottom;
					RE::NiPoint3 right_top = *mb_right_top;
					RE::NiPoint3 right_bottom = *mb_right_bottom;

					uint32_t v = processedCharacters * 6;
					vertices[v + 0] = { { left_top.x, left_top.y, left_top.z, 1.0f }, { uvLeft, uvTop }, text.color };
					vertices[v + 1] = { { left_bottom.x, left_bottom.y, left_bottom.z, 1.0f }, { uvLeft, uvBottom }, text.color };
					vertices[v + 2] = { { right_top.x, right_top.y, right_top.z, 1.0f }, { uvRight, uvTop }, text.color };
					vertices[v + 3] = { { right_top.x, right_top.y, right_top.z, 1.0f }, { uvRight, uvTop }, text.color };
					vertices[v + 4] = { { left_bottom.x, left_bottom.y, left_bottom.z, 1.0f }, { uvLeft, uvBottom }, text.color };
					vertices[v + 5] = { { right_bottom.x, right_bottom.y, right_bottom.z, 1.0f }, { uvRight, uvBottom },
						text.color };

					processedCharacters += 1;
				}

				float charAdvance = glyph.advance * scale;
				current += right_dir * charAdvance;
			}

			return processedCharacters;
		}
	};

	class DrawHandler
	{
	public:
		static DrawHandler* GetSingleton()
		{
			static DrawHandler singleton;
			return std::addressof(singleton);
		}

		void Update(float a_delta)
		{
			if (RE::UI::GetSingleton()->GameIsPaused()) {
				return;
			}

			_timer += a_delta;
		}
		void Render(D3DContext& ctx)
		{
			Locker locker(_lock);

			renderables.cbufPerFrameStaging.curTime = static_cast<float>(GameTime::CurTime());
			renderables.cbufPerFrame->Update(&renderables.cbufPerFrameStaging, 0,
				sizeof(decltype(renderables.cbufPerFrameStaging)), ctx);

			renderables.cbufPerFrame->Bind(PipelineStage::Vertex, 1, ctx);
			renderables.cbufPerFrame->Bind(PipelineStage::Fragment, 1, ctx);

			const glm::mat4& matProjView = renderables.cbufPerFrameStaging.matProjView;

			SetDepthState(ctx, true, true, D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL);
			SetBlendState(ctx, true, D3D11_BLEND_OP::D3D11_BLEND_OP_ADD, D3D11_BLEND_OP::D3D11_BLEND_OP_ADD,
				D3D11_BLEND::D3D11_BLEND_SRC_ALPHA, D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND::D3D11_BLEND_ONE,
				D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA);

			if (renderables.lineSegments.size() > 0) {
				renderables.lineDrawer->Submit(renderables.lineSegments);
			}

			if (renderables.textElements.size() > 0) {
				SetDepthState(ctx, true, true, D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL);

				renderables.textRenderer->Submit(renderables.textElements, ctx, matProjView);
			}

			SetDepthState(ctx, true, true, D3D11_COMPARISON_FUNC::D3D11_COMPARISON_ALWAYS);

			if (renderables.drawOnTopLineSegments.size() > 0) {
				renderables.lineDrawer->Submit(renderables.drawOnTopLineSegments);
			}

			if (renderables.drawOnTopTextElements.size() > 0) {
				renderables.textRenderer->Submit(renderables.drawOnTopTextElements, ctx, matProjView);
			}

			std::erase_if(renderables.textElements,
				[this](const TextElement& text) { return text.timestamp + text.duration <= _timer; });
			std::erase_if(renderables.drawOnTopTextElements,
				[this](const TextElement& text) { return text.timestamp + text.duration <= _timer; });

			std::erase_if(renderables.lineSegments,
				[this](const Line& line) { return line.timestamp + line.duration <= _timer; });
			std::erase_if(renderables.drawOnTopLineSegments,
				[this](const Line& line) { return line.timestamp + line.duration <= _timer; });
		}

		static void DrawDebugTextHelper(TextElement text)
		{
			if (text.text.size() > TextRenderer::CharsPerBatch)
				return;

			text.timestamp = DrawHandler::GetTime();
			auto drawHandler = DrawHandler::GetSingleton();
			Locker locker(drawHandler->_lock);

			auto& list = text.drawOnTop ? drawHandler->renderables.drawOnTopTextElements : drawHandler->renderables.textElements;
			list.push_back(std::move(text));
		}

		static void DrawDebugText(const RE::NiPoint3& a_position, const std::string& a_text, float a_duration,
			glm::vec4 a_color = glm::vec4(1.0f), float a_fontSize = 16.0f, bool a_drawOnTop = false)
		{
			DrawDebugTextHelper(TextElement(a_text, a_position, a_color, a_fontSize, 0, a_duration, {}, a_drawOnTop, false));
		}

		static void DrawDebugTextNorm(const RE::NiPoint3& a_position, const RE::NiPoint3& a_normal, const std::string& a_text,
			float a_duration, glm::vec4 a_color = glm::vec4(1.0f), float a_fontSize = 16.0f, bool a_drawOnTop = false)
		{
			DrawDebugTextHelper(TextElement(a_text, a_position, a_color, a_fontSize, 0, a_duration, a_normal, a_drawOnTop, true));
		}

		static void DrawDebugLine(const RE::NiPoint3& a_start, const RE::NiPoint3& a_end, float a_duration, glm::vec4 a_color,
			bool a_drawOnTop = false)
		{
			glm::vec3 start{ a_start.x, a_start.y, a_start.z };
			glm::vec3 end{ a_end.x, a_end.y, a_end.z };
			double timestamp = DrawHandler::GetTime();

			Line line(Point(start, a_color), Point(end, a_color), timestamp, a_duration);

			auto drawHandler = DrawHandler::GetSingleton();

			Locker locker(drawHandler->_lock);

			auto& list = a_drawOnTop ? drawHandler->renderables.drawOnTopLineSegments : drawHandler->renderables.lineSegments;
			list.emplace_back(line);
		}
		static void DrawDebugPoint(const RE::NiPoint3& a_position, float a_duration, glm::vec4 a_color, bool a_drawOnTop = false)
		{
			glm::vec3 position{ a_position.x, a_position.y, a_position.z };
			double timestamp = DrawHandler::GetTime();

			const glm::vec3 offset1{ 2, 2, 0.f };
			const glm::vec3 offset2{ 2, -2, 0.f };

			Line line1(Point(position - offset1, a_color), Point(position + offset1, a_color), timestamp, a_duration);
			Line line2(Point(position - offset2, a_color), Point(position + offset2, a_color), timestamp, a_duration);

			auto drawHandler = DrawHandler::GetSingleton();

			Locker locker(drawHandler->_lock);

			if (a_drawOnTop) {
				drawHandler->renderables.drawOnTopLineSegments.emplace_back(line1);
				drawHandler->renderables.drawOnTopLineSegments.emplace_back(line2);
			} else {
				drawHandler->renderables.lineSegments.emplace_back(line1);
				drawHandler->renderables.lineSegments.emplace_back(line2);
			}
		}
		static void DrawCircle(const RE::NiPoint3& a_base, const RE::NiPoint3& a_X, const RE::NiPoint3& a_Y, float a_radius,
			uint8_t a_numSides, float a_duration, glm::vec4 a_color, bool a_drawOnTop = false)
		{
			const float angleDelta = 2.0f * glm::pi<float>() / a_numSides;
			RE::NiPoint3 lastVertex = a_base + a_X * a_radius;

			for (int i = 0; i < a_numSides; i++) {
				const RE::NiPoint3 vertex =
					a_base + (a_X * cosf(angleDelta * (i + 1)) + a_Y * sinf(angleDelta * (i + 1))) * a_radius;
				DrawDebugLine(lastVertex, vertex, a_duration, a_color, a_drawOnTop);
				lastVertex = vertex;
			}
		}
		static void DrawHalfCircle(const RE::NiPoint3& a_base, const RE::NiPoint3& a_X, const RE::NiPoint3& a_Y, float a_radius,
			uint8_t a_numSides, float a_duration, glm::vec4 a_color, bool a_drawOnTop = false)
		{
			const float angleDelta = 2.0f * glm::pi<float>() / a_numSides;
			RE::NiPoint3 lastVertex = a_base + a_X * a_radius;

			for (int i = 0; i < (a_numSides / 2); i++) {
				const RE::NiPoint3 vertex =
					a_base + (a_X * cosf(angleDelta * (i + 1)) + a_Y * sinf(angleDelta * (i + 1))) * a_radius;
				DrawDebugLine(lastVertex, vertex, a_duration, a_color, a_drawOnTop);
				lastVertex = vertex;
			}
		}
		static void DrawDebugCapsule(const RE::NiPoint3& a_vertexA, const RE::NiPoint3& a_vertexB, float a_radius,
			float a_duration, glm::vec4 a_color, bool a_drawOnTop = false)
		{
			constexpr int32_t collisionSides = 16;

			RE::NiPoint3 zAxis = a_vertexA - a_vertexB;
			zAxis.Unitize();

			// get other axis
			RE::NiPoint3 upVector =
				(fabs(zAxis.z) < (1.f - 1.e-4f)) ? RE::NiPoint3{ 0.f, 0.f, 1.f } : RE::NiPoint3{ 1.f, 0.f, 0.f };
			RE::NiPoint3 xAxis = upVector.UnitCross(zAxis);
			RE::NiPoint3 yAxis = zAxis.Cross(xAxis);

			// draw top and bottom circles
			DrawCircle(a_vertexA, xAxis, yAxis, a_radius, collisionSides, a_duration, a_color);
			DrawCircle(a_vertexB, xAxis, yAxis, a_radius, collisionSides, a_duration, a_color);

			// draw caps
			DrawHalfCircle(a_vertexA, yAxis, zAxis, a_radius, collisionSides, a_duration, a_color, a_drawOnTop);
			DrawHalfCircle(a_vertexA, xAxis, zAxis, a_radius, collisionSides, a_duration, a_color, a_drawOnTop);

			RE::NiPoint3 negZAxis = -zAxis;

			DrawHalfCircle(a_vertexB, yAxis, negZAxis, a_radius, collisionSides, a_duration, a_color, a_drawOnTop);
			DrawHalfCircle(a_vertexB, xAxis, negZAxis, a_radius, collisionSides, a_duration, a_color, a_drawOnTop);

			// draw connected lines
			RE::NiPoint3 start, end;
			start = a_vertexA + xAxis * a_radius;
			end = a_vertexB + xAxis * a_radius;
			DrawDebugLine(start, end, a_duration, a_color, a_drawOnTop);
			start = a_vertexA - xAxis * a_radius;
			end = a_vertexB - xAxis * a_radius;
			DrawDebugLine(start, end, a_duration, a_color, a_drawOnTop);
			start = a_vertexA + yAxis * a_radius;
			end = a_vertexB + yAxis * a_radius;
			DrawDebugLine(start, end, a_duration, a_color, a_drawOnTop);
			start = a_vertexA - yAxis * a_radius;
			end = a_vertexB - yAxis * a_radius;
			DrawDebugLine(start, end, a_duration, a_color, a_drawOnTop);

			//draw bone axis
			auto capsuleCenter = (a_vertexA + a_vertexB) / 2;
			const glm::vec4 xColor{ 1, 0, 0, 1 };
			const glm::vec4 yColor{ 0, 1, 0, 1 };
			const glm::vec4 zColor{ 0, 0, 1, 1 };
			DrawDebugLine(capsuleCenter, capsuleCenter + xAxis * 5.f, a_duration, xColor, a_drawOnTop);
			DrawDebugLine(capsuleCenter, capsuleCenter + yAxis * 5.f, a_duration, yColor, a_drawOnTop);
			DrawDebugLine(capsuleCenter, capsuleCenter + zAxis * 5.f, a_duration, zColor, a_drawOnTop);
		}
		static void DrawDebugSphere(const RE::NiPoint3& a_center, float a_radius, float a_duration, glm::vec4 a_color,
			bool a_drawOnTop = false)
		{
			constexpr int32_t collisionSides = 16;

			constexpr RE::NiPoint3 xAxis{ 1.f, 0.f, 0.f };
			constexpr RE::NiPoint3 yAxis{ 0.f, 1.f, 0.f };
			constexpr RE::NiPoint3 zAxis{ 0.f, 0.f, 1.f };

			DrawCircle(a_center, xAxis, yAxis, a_radius, collisionSides, a_duration, a_color, a_drawOnTop);
			DrawCircle(a_center, xAxis, zAxis, a_radius, collisionSides, a_duration, a_color, a_drawOnTop);
			DrawCircle(a_center, yAxis, zAxis, a_radius, collisionSides, a_duration, a_color, a_drawOnTop);
		}

		static double GetTime() { return DrawHandler::GetSingleton()->_timer; }

		void OnPreLoadGame() { renderables.ClearLists(); }
		void OnSettingsUpdated()
		{
			// Hook only if debug display is enabled
			if (!_bDXHooked) {
				InstallHooks();
				assert(HasContext());
				_bDXHooked = true;
				Initialize();
			}
		}

	private:
		using Lock = std::recursive_mutex;
		using Locker = std::lock_guard<Lock>;

		DrawHandler() : _lock() {}
		DrawHandler(const DrawHandler&) = delete;
		DrawHandler(DrawHandler&&) = delete;
		virtual ~DrawHandler() = default;

		DrawHandler& operator=(const DrawHandler&) = delete;
		DrawHandler& operator=(DrawHandler&&) = delete;

		mutable Lock _lock;

		void Initialize()
		{
			if (!_bDXHooked)
				return;
			auto& ctx = GetContext();

			// Per-object data, changing each draw call (model)
			CBufferCreateInfo perObj;
			perObj.bufferUsage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
			perObj.cpuAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
			perObj.size = sizeof(decltype(renderables.cbufPerObjectStaging));
			perObj.initialData = &renderables.cbufPerObjectStaging;
			renderables.cbufPerObject = std::make_shared<CBuffer>(perObj, ctx);

			// Per-frame data, shared among many objects (view, projection)
			CBufferCreateInfo perFrane;
			perFrane.bufferUsage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
			perFrane.cpuAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
			perFrane.size = sizeof(decltype(renderables.cbufPerFrameStaging));
			perFrane.initialData = &renderables.cbufPerFrameStaging;
			renderables.cbufPerFrame = std::make_shared<CBuffer>(perFrane, ctx);

			renderables.lineDrawer = std::make_unique<LineDrawer>(ctx);
			renderables.textRenderer = std::make_unique<TextRenderer>(ctx);

			if (HasContext()) {
				OnPresent(std::bind(&DrawHandler::Render, this, std::placeholders::_1));
			}
		}

		bool _bDXHooked = false;

		// Renderable objects
		struct
		{
			// Data which should really only change once each frame
			struct VSMatricesCBuffer
			{
				glm::mat4 matProjView = glm::identity<glm::mat4>();
				float curTime = 0.0f;
				float pad[3] = { 0.0f, 0.0f, 0.0f };
			};
			static_assert(sizeof(VSMatricesCBuffer) % 16 == 0);

			// Data which is likely to change each draw call
			struct VSPerObjectCBuffer
			{
				glm::mat4 model = glm::identity<glm::mat4>();
			};
			static_assert(sizeof(VSMatricesCBuffer) % 16 == 0);

			VSMatricesCBuffer cbufPerFrameStaging = {};
			VSPerObjectCBuffer cbufPerObjectStaging = {};
			std::shared_ptr<CBuffer> cbufPerFrame;
			std::shared_ptr<CBuffer> cbufPerObject;

			// Resources for drawing the debug line
			LineList lineSegments;
			LineList drawOnTopLineSegments;
			std::unique_ptr<LineDrawer> lineDrawer;

			std::unique_ptr<TextRenderer> textRenderer;
			std::vector<TextElement> textElements;
			std::vector<TextElement> drawOnTopTextElements;

			void ClearLists()
			{
				lineSegments.clear();
				drawOnTopLineSegments.clear();
				textElements.clear();
				drawOnTopTextElements.clear();
			}

			// D3D expects resources to be released in a certain order
			void release()
			{
				lineDrawer.reset();
				cbufPerObject.reset();
				cbufPerFrame.reset();
				textRenderer.reset();
			}
		} renderables;

		double _timer = 0.0;
	};

	void UpdateHooks::Nullsub()
	{
		_Nullsub();

		float* g_deltaTime = (float*)REL::ID(523660).address();
		DrawHandler::GetSingleton()->Update(*g_deltaTime);
	}

	void OnMessage(SKSE::MessagingInterface::Message* message)
	{
		switch (message->type) {
		case SKSE::MessagingInterface::kPreLoadGame:
			DrawHandler::GetSingleton()->OnPreLoadGame();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			DrawHandler::GetSingleton()->OnSettingsUpdated();
			break;
		}
	}

	namespace DrawDebug
	{
		[[maybe_unused]] static void draw_text_plane(const RE::NiPoint3& position, const RE::NiPoint3& normal, float duration, bool drawOnTop)
		{
			RE::NiPoint3 normal_dir = normal;
			normal_dir.Unitize();
			float w = 50;
			float h = 20;
			RE::NiPoint3 up_dir = { 0, 0, 1 };
			RE::NiPoint3 right = up_dir.Cross(normal);
			RE::NiPoint3 right_dir;
			float right_len = right.Length();
			if (right_len < 0.001f) {
				right_dir = RE::NiPoint3(1, 0, 0);
			} else {
				right_dir = right / right_len;
			}

			RE::NiPoint3 vs[4];
			vs[0] = position + up_dir * h + right_dir * w;
			vs[1] = position - up_dir * h + right_dir * w;
			vs[2] = position - up_dir * h - right_dir * w;
			vs[3] = position + up_dir * h - right_dir * w;
			draw_line(vs[0], vs[1], Colors::BLU, duration, drawOnTop);
			draw_line(vs[1], vs[2], Colors::BLU, duration, drawOnTop);
			draw_line(vs[2], vs[3], Colors::BLU, duration, drawOnTop);
			draw_line(vs[3], vs[0], Colors::BLU, duration, drawOnTop);

			draw_line(position + right_dir * w, position - right_dir * w, Colors::GRN, duration, drawOnTop);
		}
		
		void draw_text(const RE::NiPoint3& position, const std::string& text, glm::vec4 color, float size, float duration,
			bool drawOnTop)
		{
			DrawHandler::DrawDebugText(position, text, duration, color, size, drawOnTop);
		}

		void draw_text0(const RE::NiPoint3& position, const std::string& text, glm::vec4 color, float size, bool drawOnTop)
		{
			draw_text(position, text, color, size, 0, drawOnTop);
		}

		void draw_text_norm(const RE::NiPoint3& position, const RE::NiPoint3& normal, const std::string& text, glm::vec4 color,
			float duration, float size, bool drawOnTop)
		{
			DrawHandler::DrawDebugTextNorm(position, normal, text, duration, color, size, drawOnTop);
		}

		void draw_text_norm0(const RE::NiPoint3& position, const RE::NiPoint3& normal, const std::string& text, glm::vec4 color,
			float size, bool drawOnTop)
		{
			draw_text_norm(position, normal, text, color, 0, size, drawOnTop);
		}

		void draw_line(const RE::NiPoint3& start, const RE::NiPoint3& end, glm::vec4 color, float duration, bool drawOnTop)
		{
			DrawHandler::DrawDebugLine(start, end, duration, color, drawOnTop);
		}

		void draw_line0(const RE::NiPoint3& start, const RE::NiPoint3& end, glm::vec4 color, bool drawOnTop)
		{
			draw_line(start, end, color, 0, drawOnTop);
		}

		void draw_vector(const RE::NiPoint3& start, const RE::NiPoint3& V, float len, glm::vec4 color, float duration,
			bool drawOnTop)
		{
			draw_line(start, start + V * len, color, duration, drawOnTop);
		}

		void draw_vector0(const RE::NiPoint3& start, const RE::NiPoint3& V, float len, glm::vec4 color, bool drawOnTop)
		{
			draw_line0(start, start + V * len, color, drawOnTop);
		}

		void draw_sphere(const RE::NiPoint3& center, float radius, glm::vec4 color, float duration, bool drawOnTop)
		{
			DrawHandler::DrawDebugSphere(center, radius, duration, color, drawOnTop);
		}

		void draw_sphere0(const RE::NiPoint3& center, float radius, glm::vec4 color, bool drawOnTop)
		{
			draw_sphere(center, radius, color, 0, drawOnTop);
		}

		void draw_point(const RE::NiPoint3& position, glm::vec4 color, float duration, bool drawOnTop)
		{
			DrawHandler::DrawDebugPoint(position, duration, color, drawOnTop);
		}

		void draw_point0(const RE::NiPoint3& position, glm::vec4 color, bool drawOnTop)
		{
			draw_point(position, color, 0, drawOnTop);
		}

		void draw_shape(const RE::CombatMathUtilities::Capsule& cap, glm::vec4 color, float duration, bool drawOnTop)
		{
			DrawHandler::DrawDebugCapsule(cap.segment.base, cap.segment.base + cap.segment.offset, cap.radius, duration, color,
				drawOnTop);
		}

		void draw_shape0(const RE::CombatMathUtilities::Capsule& cap, glm::vec4 color, bool drawOnTop)
		{
			draw_shape(cap, color, 0, drawOnTop);
		}

		void draw_shape(const RE::CombatMathUtilities::Sphere& s, glm::vec4 color, float duration, bool drawOnTop)
		{
			draw_sphere(s.center, s.radius, color, duration, drawOnTop);
		}

		void draw_shape0(const RE::CombatMathUtilities::Sphere& s, glm::vec4 color, bool drawOnTop)
		{
			draw_shape(s, color, 0, drawOnTop);
		}

		void draw_shape(RE::CombatMathUtilities::MovingCapsule cap, glm::vec4 color, float duration, bool drawOnTop)
		{
			draw_shape(cap.inner, color, duration, drawOnTop);
			cap.inner.segment.base += cap.translation * 0.5f;
			draw_shape(cap.inner, color, duration, drawOnTop);
			cap.inner.segment.base += cap.translation * 0.5f;
			draw_shape(cap.inner, color, duration, drawOnTop);
		}

		void draw_shape0(RE::CombatMathUtilities::MovingCapsule cap, glm::vec4 color, bool drawOnTop)
		{
			draw_shape(cap, color, 0, drawOnTop);
		}

		void draw_shape(RE::CombatMathUtilities::MovingSphere cap, glm::vec4 color, float duration, bool drawOnTop)
		{
			draw_shape(cap.inner, color, duration, drawOnTop);
			cap.inner.center += cap.translation * 0.5f;
			draw_shape(cap.inner, color, duration, drawOnTop);
			cap.inner.center += cap.translation * 0.5f;
			draw_shape(cap.inner, color, duration, drawOnTop);
		}

		void draw_shape0(RE::CombatMathUtilities::MovingSphere cap, glm::vec4 color, bool drawOnTop)
		{
			draw_shape(cap, color, 0, drawOnTop);
		}
	}
}

#pragma warning(pop)
