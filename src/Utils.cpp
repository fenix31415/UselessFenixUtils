#include <UselessFenixUtils.h>

#include <d3dcompiler.h>
#include <timeapi.h>
#include <windows.h>
#pragma comment(lib, "winmm.lib")

#pragma warning(disable: 4189)
#include <d3d11.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <winrt/base.h>
#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#pragma warning(disable: 4244 4267)

#include "Utils1.h"

namespace DebugRender
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
			std::string&& version = "5_0") :
			source(source),
			entryName(entryName), version(version), stage(stage)
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

		std::unordered_map<BlendStateKey, BlendState, BlendStateHasher, BlendStateCompare>
			loadedBlendStates;

		std::unordered_map<RasterStateKey, RasterState, RasterStateHasher, RasterStateCompare>
			loadedRasterStates;

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
		if (!ReadSwapChain()) {
			logger::error("SmoothCam: Failed to hook IDXGISwapChain::Present and aquire device context, drawing is disabled.");
			return;
		}

		if (!SUCCEEDED(gameContext.swapChain->GetDevice(__uuidof(ID3D11Device), gameContext.device.put_void()))) {
			logger::error("SmoothCam: Failed to get rendering device.");
			return;
		}

		REL::Relocation<std::uintptr_t> D3DVtbl{ *(uintptr_t*)gameContext.swapChain };
		fnPresentOrig = D3DVtbl.write_vfunc(0x8, Present);

		gameContext.device->GetImmediateContext(gameContext.context.put());
		initialized = true;
	}

	// Shutdown, release references
	void Shutdown()
	{
		if (!initialized)
			return;
		initialized = false;

		// Release program lifetime objects
		d3dObjects.release();

		// Free our present hook
		//d3dHook->unHook();

		// Explicit release
		gameContext.context = nullptr;
		gameContext.device = nullptr;
	}

	// Get the game's D3D context
	D3DContext& GetContext() noexcept { return gameContext; }

	// Returns true if we have a valid D3D context
	bool HasContext() noexcept { return initialized; }
	// Get the game's depth-stencil view for the back buffer
	winrt::com_ptr<ID3D11DepthStencilView>& GetDepthStencilView() noexcept { return d3dObjects.depthStencilView; }
	// Get the game's render target for the back buffer
	winrt::com_ptr<ID3D11RenderTargetView>& GetGameRT() noexcept { return d3dObjects.gameRTV; }

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

	// Set the raster state
	void SetRasterState(D3DContext& ctx, D3D11_FILL_MODE fillMode, D3D11_CULL_MODE cullMode, bool frontCCW, int32_t depthBias = 0,
		float depthBiasClamp = 0.0f, float slopeScaledDepthBias = 0.0f, bool lineAA = true, bool depthClip = false,
		bool scissorEnable = false, bool msaa = false)
	{
		D3D11_RASTERIZER_DESC desc;
		desc.FillMode = fillMode;
		desc.CullMode = cullMode;
		desc.FrontCounterClockwise = frontCCW;
		desc.DepthBias = depthBias;
		desc.DepthBiasClamp = depthBiasClamp;
		desc.SlopeScaledDepthBias = slopeScaledDepthBias;
		desc.DepthClipEnable = depthClip;
		desc.ScissorEnable = scissorEnable;
		desc.MultisampleEnable = msaa;
		desc.AntialiasedLineEnable = lineAA;
		auto key = RasterStateKey{ desc };

		auto it = d3dObjects.loadedRasterStates.find(key);
		if (it != d3dObjects.loadedRasterStates.end()) {
			ctx.context->RSSetState(it->second.state.get());
			return;
		}

		auto state = RasterState{ ctx, key };
		ctx.context->RSSetState(state.state.get());
		d3dObjects.loadedRasterStates.emplace(key, std::move(state));
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
					logger::error("SmoothCam: A shader failed to compile.");
			} else {
				auto result = context.device->CreatePixelShader(binary->GetBufferPointer(), binary->GetBufferSize(), nullptr,
					&program.fragment);
				validProgram = SUCCEEDED(result);
				if (!validProgram)
					logger::error("SmoothCam: A shader failed to compile.");
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
				logger::critical("SmoothCam: Failed to map a D3D vertex buffer.");
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
				logger::critical("SmoothCam: Failed to create input assembler layout.");
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
				logger::critical("SmoothCam: Failed to create D3D vertex buffer.");
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
				logger::critical("SmoothCam: Failed to create D3D cbuffer.");
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
				logger::critical("SmoothCam: Failed to map cbuffer resource.");

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

		DrawnPoint(Point&& a_point, double& a_timestamp, float& a_duration) :
			point(a_point), timestamp(a_timestamp), duration(a_duration){};
	} DrawnPoint;

	typedef struct Line
	{
		Point start;
		Point end;
		float duration;
		double timestamp;
		Line(Point&& a_start, Point&& a_end, double& a_timestamp, float& a_duration) :
			start(a_start), end(a_end), timestamp(a_timestamp), duration(a_duration){
				//timestamp = static_cast<float>(GameTime::CurTime());
			};
	} Line;

	typedef struct Collider
	{
		RE::NiPointer<RE::NiAVObject> node;
		float duration;
		double timestamp;
		glm::vec4 color;
		Collider(RE::NiPointer<RE::NiAVObject> node, double& timestamp, float& duration, glm::vec4& color) :
			node(node), timestamp(timestamp), duration(duration), color(color){};

		[[nodiscard]] friend bool operator==(const Collider& a_lhs, const Collider& a_rhs) noexcept
		{
			return a_lhs.node == a_rhs.node;
		}

		[[nodiscard]] friend bool operator==(const Collider& a_this, const RE::NiPointer<RE::NiAVObject> a_node) noexcept
		{
			return a_this.node == a_node;
		}
	} Collider;

	using LineList = std::vector<Line>;
	using PointList = std::vector<DrawnPoint>;
	using ColliderList = std::vector<Collider>;

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

	void DrawCollider(RE::NiAVObject* a_node, [[maybe_unused]] float a_duration, [[maybe_unused]] glm::vec4 a_color);
	void DrawColliders(RE::NiAVObject* a_node, float a_duration, glm::vec4 a_color, bool a_bCheckJumpIframes /*= false*/);

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
			//GameTime::StepFrameTime();
			if (RE::UI::GetSingleton()->GameIsPaused()) {
				return;
			}

			_timer += a_delta;
		}
		void Render(D3DContext& ctx)
		{
			//auto playerCamera = RE::PlayerCamera::GetSingleton();
			//auto currentCameraState = playerCamera->currentState;
			//if (!currentCameraState) {
			//	return;
			//}

			//

			//RE::NiPoint3 currentCameraPosition;
			//currentCameraState->GetTranslation(currentCameraPosition);
			//glm::vec3 cameraPosition(currentCameraPosition.x, currentCameraPosition.y, currentCameraPosition.z);

			//RE::NiQuaternion niq;
			//currentCameraState->GetRotation(niq);
			//glm::quat q {niq.w, niq.x, niq.y, niq.z};
			//glm::vec2 cameraRotation;
			//cameraRotation.x = glm::pitch(q) * -1.0f;
			//cameraRotation.y = glm::roll(q) * -1.0f;  // The game stores yaw in the Z axis

			//RE::NiPointer<RE::NiCamera> niCamera = RE::NiPointer<RE::NiCamera>(static_cast<RE::NiCamera*>(playerCamera->cameraRoot->children[0].get()));

			//const auto matProj = Render::GetProjectionMatrix(niCamera->viewFrustum);
			//const auto matView = Render::BuildViewMatrix(cameraPosition, cameraRotation);
			//const auto matWorldToCam = *reinterpret_cast<glm::mat4*>(g_worldToCamMatrix);

			//renderables.cbufPerFrameStaging.matProjView = matProj * matView;

			Locker locker(_lock);

			renderables.cbufPerFrameStaging.curTime = static_cast<float>(GameTime::CurTime());
			renderables.cbufPerFrame->Update(&renderables.cbufPerFrameStaging, 0,
				sizeof(decltype(renderables.cbufPerFrameStaging)), ctx);

			// Bind at register b1
			renderables.cbufPerFrame->Bind(PipelineStage::Vertex, 1, ctx);
			renderables.cbufPerFrame->Bind(PipelineStage::Fragment, 1, ctx);

			// Setup depth and blending
			SetDepthState(ctx, true, true, D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL);
			SetBlendState(ctx, true, D3D11_BLEND_OP::D3D11_BLEND_OP_ADD, D3D11_BLEND_OP::D3D11_BLEND_OP_ADD,
				D3D11_BLEND::D3D11_BLEND_SRC_ALPHA, D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND::D3D11_BLEND_ONE,
				D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA);

			if (renderables.lineSegments.size() > 0) {
				renderables.lineDrawer->Submit(renderables.lineSegments);
				//renderables.lineSegments.clear();
			}

			SetDepthState(ctx, true, true, D3D11_COMPARISON_FUNC::D3D11_COMPARISON_ALWAYS);

			if (renderables.drawOnTopLineSegments.size() > 0) {
				renderables.lineDrawer->Submit(renderables.drawOnTopLineSegments);
			}

			//renderables.colliders.erase(std::remove_if(renderables.colliders.begin(), renderables.colliders.end(), [this](Render::Collider collider) { return collider.timestamp + collider.duration < _timer; }), renderables.colliders.end());
			std::erase_if(renderables.colliders, [this](Collider collider) {
				return collider.duration > 0 && collider.timestamp + collider.duration < _timer;
			});
			DrawDebugCapsules();

			std::erase_if(renderables.points,
				[this](DrawnPoint point) { return point.duration > 0 && point.timestamp + point.duration < _timer; });
			std::erase_if(renderables.drawOnTopPoints,
				[this](DrawnPoint point) { return point.duration > 0 && point.timestamp + point.duration < _timer; });
			DrawPoints();

			// remove expired lines
			//renderables.lineSegments.erase(std::remove_if(renderables.lineSegments.begin(), renderables.lineSegments.end(), [this](Render::Line line) { return line.timestamp + line.duration < _timer; }), renderables.lineSegments.end());
			std::erase_if(renderables.lineSegments,
				[this](Line line) { return line.timestamp + line.duration < _timer; });
			//renderables.drawOnTopLineSegments.erase(std::remove_if(renderables.drawOnTopLineSegments.begin(), renderables.drawOnTopLineSegments.end(), [this](Render::Line line) { return line.timestamp + line.duration < _timer; }), renderables.drawOnTopLineSegments.end());
			std::erase_if(renderables.drawOnTopLineSegments,
				[this](Line line) { return line.timestamp + line.duration < _timer; });
		}

		void DrawDebugCapsules()
		{
			for (auto& collider : renderables.colliders) {
				DrawCollider(collider.node.get(), 0.f, collider.color);
			}
		}
		void DrawPoints()
		{
			for (auto& point : renderables.points) {
				DrawDebugPoint(RE::NiPoint3(point.point.pos.x, point.point.pos.y, point.point.pos.z), 0.f, point.point.col);
			}

			for (auto& point : renderables.drawOnTopPoints) {
				DrawDebugPoint(RE::NiPoint3(point.point.pos.x, point.point.pos.y, point.point.pos.z), 0.f, point.point.col, true);
			}
		}

		static bool isLineClose(const Line& line, DebugRender::LineList& a)
		{
			for (auto& l : a) {
				if (l.start.col == line.start.col && l.end.col == line.end.col) {
					const float eps = 10.0f;
					if (glm::length(line.start.pos - l.start.pos) < eps && glm::length(line.end.pos - l.end.pos) < eps) {
						l = line;
						return true;
					}
				}
			}
			return false;
		}

		static void DrawDebugLine(const RE::NiPoint3& a_start, const RE::NiPoint3& a_end, float a_duration, glm::vec4 a_color,
			bool a_drawOnTop = false)
		{
			glm::vec3 start{ a_start.x, a_start.y, a_start.z };
			glm::vec3 end{ a_end.x, a_end.y, a_end.z };
			double timestamp = DrawHandler::GetTime();

			//RE::NiCamera::WorldPtToScreenPt3((float(*)[4])g_worldToCamMatrix, *g_viewPort, a_start, start.x, start.y, start.z,
			//	1e-5f);
			//start -= 0.5f;
			//start *= 2.f;
			//
			//if (start.z < 0) {
			//	return;
			//}

			//RE::NiCamera::WorldPtToScreenPt3((float(*)[4])g_worldToCamMatrix, *g_viewPort, a_end, end.x, end.y, end.z, 1e-5f);
			//end -= 0.5f;
			//end *= 2.f;
			//
			//if (end.z < 0) {
			//	return;
			//}

			Line line(Point(start, a_color), Point(end, a_color), timestamp, a_duration);

			auto drawHandler = DrawHandler::GetSingleton();

			Locker locker(drawHandler->_lock);

			auto& list = a_drawOnTop ? drawHandler->renderables.drawOnTopLineSegments : drawHandler->renderables.lineSegments;
			//if (!isLineClose(line, list)) {
				list.emplace_back(line);
			//}
		}
		static void DrawDebugPoint(const RE::NiPoint3& a_position, float a_duration, glm::vec4 a_color, bool a_drawOnTop = false)
		{
			glm::vec3 position{ a_position.x, a_position.y, a_position.z };
			double timestamp = DrawHandler::GetTime();

			//RE::NiCamera::WorldPtToScreenPt3((float(*)[4])g_worldToCamMatrix, *g_viewPort, a_position, position.x, position.y,
			//	position.z, 1e-5f);
			//position -= 0.5f;
			//position *= 2.f;
			//
			//if (position.z < 0) {
			//	return;
			//}

			const glm::vec3 offset1{ 2, 2, 0.f };
			const glm::vec3 offset2{ 2, -2, 0.f };

			Line line1(Point(position - offset1, a_color), Point(position + offset1, a_color), timestamp,
				a_duration);
			Line line2(Point(position - offset2, a_color), Point(position + offset2, a_color), timestamp,
				a_duration);

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

		static void DrawSweptCapsule(const RE::NiPoint3& a_a, const RE::NiPoint3& a_b, const RE::NiPoint3& a_c,
			const RE::NiPoint3& a_d, float, const RE::NiMatrix3&, float a_duration, glm::vec4 a_color,
			bool a_drawOnTop = false)
		{
			DrawDebugLine(a_a, a_b, a_duration, a_color, a_drawOnTop);
			DrawDebugLine(a_b, a_c, a_duration, a_color, a_drawOnTop);
			DrawDebugLine(a_c, a_d, a_duration, a_color, a_drawOnTop);
			DrawDebugLine(a_d, a_a, a_duration, a_color, a_drawOnTop);

			//constexpr int32_t collisionSides = 16;

			//RE::NiPoint3 x{ 1.f, 0.f, 0.f };
			//RE::NiPoint3 y{ 0.f, 1.f, 0.f };
			//RE::NiPoint3 z{ 0.f, 0.f, 1.f };
			//x = Utils::TransformVectorByMatrix(x, a_rotation);
			//y = Utils::TransformVectorByMatrix(y, a_rotation);
			//z = Utils::TransformVectorByMatrix(z, a_rotation);

			//// top
			//DrawDebugLine(a_a + z * a_radius, a_b + z * a_radius, a_duration, a_color, true);
			//DrawDebugLine(a_b + z * a_radius, a_c + z * a_radius, a_duration, a_color, true);
			//DrawDebugLine(a_c + z * a_radius, a_d + z * a_radius, a_duration, a_color, true);
			//DrawDebugLine(a_d + z * a_radius, a_a + z * a_radius, a_duration, a_color, true);

			//// bottom
			//DrawDebugLine(a_a - z * a_radius, a_b - z * a_radius, a_duration, a_color, true);
			//DrawDebugLine(a_b - z * a_radius, a_c - z * a_radius, a_duration, a_color, true);
			//DrawDebugLine(a_c - z * a_radius, a_d - z * a_radius, a_duration, a_color, true);
			//DrawDebugLine(a_d - z * a_radius, a_a - z * a_radius, a_duration, a_color, true);

			//// sides
			//DrawDebugLine(a_a - x * a_radius, a_b - x * a_radius, a_duration, a_color, true);
			//DrawDebugLine(a_b + y * a_radius, a_c + y * a_radius, a_duration, a_color, true);
			//DrawDebugLine(a_c + x * a_radius, a_d + x * a_radius, a_duration, a_color, true);
			//DrawDebugLine(a_d - y * a_radius, a_a - y * a_radius, a_duration, a_color, true);
		}

		static void AddCollider(const RE::NiPointer<RE::NiAVObject> a_node, float a_duration, glm::vec4 a_color)
		{
			double timestamp = DrawHandler::GetTime();
			Collider collider(a_node, timestamp, a_duration, a_color);
			auto drawHandler = DrawHandler::GetSingleton();

			Locker locker(drawHandler->_lock);
			if (std::find(drawHandler->renderables.colliders.begin(), drawHandler->renderables.colliders.end(), collider) ==
				drawHandler->renderables.colliders.end()) {
				drawHandler->renderables.colliders.emplace_back(collider);
			}
		}
		static void RemoveCollider(const RE::NiPointer<RE::NiAVObject> a_node)
		{
			auto drawHandler = DrawHandler::GetSingleton();

			Locker locker(drawHandler->_lock);
			auto it = std::find(drawHandler->renderables.colliders.begin(), drawHandler->renderables.colliders.end(), a_node);
			if (it != drawHandler->renderables.colliders.end()) {
				drawHandler->renderables.colliders.erase(it);
			}
		}

		static void AddPoint(const RE::NiPoint3& a_point, float a_duration, glm::vec4 a_color, bool a_bDrawOnTop = false)
		{
			double timestamp = DrawHandler::GetTime();
			DrawnPoint point(Point(glm::vec3(a_point.x, a_point.y, a_point.z), a_color), timestamp, a_duration);
			auto drawHandler = DrawHandler::GetSingleton();

			Locker locker(drawHandler->_lock);
			if (a_bDrawOnTop) {
				drawHandler->renderables.drawOnTopPoints.emplace_back(point);
			} else {
				drawHandler->renderables.points.emplace_back(point);
			}
		}

		static double GetTime() { return DrawHandler::GetSingleton()->_timer; }

		void OnPreLoadGame() { renderables.ClearLists(); }
		void OnSettingsUpdated()
		{
			// Hook only if debug display is enabled
			if (!_bDXHooked) {
				InstallHooks();
				if (HasContext()) {
					_bDXHooked = true;
					Initialize();
				} else {
					logger::critical(
						"Precision: Failed to hook DirectX, Rendering features will be disabled. Try running with overlay "
					    "software "
						"disabled if this warning keeps occurring.");
				}
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

			PointList points;
			PointList drawOnTopPoints;
			ColliderList colliders;

			void ClearLists()
			{
				lineSegments.clear();
				drawOnTopLineSegments.clear();
				points.clear();
				drawOnTopPoints.clear();
				colliders.clear();
			}

			// D3D expects resources to be released in a certain order
			void release()
			{
				lineDrawer.reset();
				cbufPerObject.reset();
				cbufPerFrame.reset();
			}
		} renderables;

		double _timer = 0.0;
	};

	struct Capsule
	{
		RE::NiPoint3 a;
		RE::NiPoint3 b;
		float radius;
	};
	
	bool GetCapsuleParams(RE::NiAVObject* a_node, Capsule& a_outCapsule)
	{
		if (a_node && a_node->collisionObject) {
			auto collisionObject = static_cast<RE::bhkCollisionObject*>(a_node->collisionObject.get());
			auto rigidBody = collisionObject->GetRigidBody();

			if (rigidBody && rigidBody->referencedObject) {
				RE::hkpRigidBody* hkpRigidBody = static_cast<RE::hkpRigidBody*>(rigidBody->referencedObject.get());
				const RE::hkpShape* hkpShape = hkpRigidBody->collidable.shape;
				if (hkpShape->type == RE::hkpShapeType::kCapsule) {
					auto hkpCapsuleShape = static_cast<const RE::hkpCapsuleShape*>(hkpShape);
					float bhkInvWorldScale = *g_worldScaleInverse;

					a_outCapsule.radius = hkpCapsuleShape->radius * bhkInvWorldScale;
					a_outCapsule.a = Utils::HkVectorToNiPoint(hkpCapsuleShape->vertexA) * bhkInvWorldScale;
					a_outCapsule.b = Utils::HkVectorToNiPoint(hkpCapsuleShape->vertexB) * bhkInvWorldScale;

					return true;
				}
			}
		}

		return false;
	}

	void DrawCollider(RE::NiAVObject* a_node, [[maybe_unused]] float a_duration, [[maybe_unused]] glm::vec4 a_color)
	{
		Capsule capsule;
		if (GetCapsuleParams(a_node, capsule)) {
			if (a_node && a_node->collisionObject) {
				auto collisionObject = static_cast<RE::bhkCollisionObject*>(a_node->collisionObject.get());
				auto rigidBody = collisionObject->GetRigidBody();
				if (rigidBody && rigidBody->referencedObject) {
					RE::hkpRigidBody* hkpRigidBody = static_cast<RE::hkpRigidBody*>(rigidBody->referencedObject.get());
					auto& hkTransform = hkpRigidBody->motion.motionState.transform;
					RE::NiTransform transform = Utils::HkTransformToNiTransform(hkTransform);

					RE::NiPoint3 vertexA = capsule.a;
					RE::NiPoint3 vertexB = capsule.b;

					vertexA = transform * vertexA;
					vertexB = transform * vertexB;

					DrawHandler::DrawDebugCapsule(vertexA, vertexB, capsule.radius, a_duration, a_color, true);
				}
			}
		}
	}

	void DrawActorColliders(RE::ActorHandle a_actorHandle, RE::NiAVObject* a_root, float a_duration, glm::vec4 a_color)
	{
		if (!a_actorHandle) {
			return;
		}

		auto actor = a_actorHandle.get();

		bool bIsGhost = actor->IsGhost();
		bool bCheckJumpIframes = false;

		const glm::vec4 blue{ 0.2, 0.2, 1.0, 1.0 };

		if (bIsGhost) {
			a_color = blue;
		}

		DrawColliders(a_root, a_duration, a_color, bCheckJumpIframes);
	}

	void DrawColliders(RE::NiAVObject* a_node, float a_duration, glm::vec4 a_color, bool a_bCheckJumpIframes /*= false*/)
	{
		if (a_node) {
			glm::vec4 nodeColor = a_color;
			if (a_bCheckJumpIframes) {
				if (!Utils::IsNodeOrChildOfNode(a_node, nullptr)) {
					const glm::vec4 blue{ 0.2, 0.2, 1.0, 1.0 };
					nodeColor = blue;
				}
			}

			DrawCollider(a_node, a_duration, nodeColor);

			auto node = a_node->AsNode();
			if (node) {
				for (auto& child : node->children) {
					DrawColliders(child.get(), a_duration, a_color, a_bCheckJumpIframes);
				}
			}
		}
	}

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
		void draw_line(const RE::NiPoint3& start, const RE::NiPoint3& end, glm::vec4 color, float duration, bool drawOnTop)
		{
			DrawHandler::DrawDebugLine(start, end, duration, color, drawOnTop);
		}

		void draw_line0(const RE::NiPoint3& start, const RE::NiPoint3& end, glm::vec4 color, bool drawOnTop)
		{
			draw_line(start, end, color, 0, drawOnTop);
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

		RE::hkbEventBase::SystemEventIDs get_implicit_id_event(RE::hkbBehaviorGraph* graph, const char* name)
		{
			const auto& names = graph->data->stringData->eventNames;
			for (int32_t i = 0; i < names.size(); i++) {
				if (!std::strcmp(names[i].c_str(), name))
					return static_cast<RE::hkbEventBase::SystemEventIDs>(i);
			}
			return RE::hkbEventBase::SystemEventIDs::kNull;
		}

		const char* get_event_name_implicit(RE::hkbBehaviorGraph* graph, RE::hkbEventBase::SystemEventIDs implicit_id)
		{
			if (implicit_id == RE::hkbEventBase::SystemEventIDs::kNull)
				return nullptr;

			return graph->data->stringData->eventNames[static_cast<uint32_t>(implicit_id)].c_str();
		}

		const char* get_event_name_explicit(RE::hkbBehaviorGraph* graph, int32_t explicit_id)
		{
			if (auto map = graph->eventIDMap.get()) {
				auto implicit_id = map->map.getWithDefault(static_cast<int64_t>(explicit_id) + 1, -1);
				if (implicit_id != -1) {
					return get_event_name_implicit(graph, static_cast<RE::hkbEventBase::SystemEventIDs>(implicit_id));
				}
			}

			return nullptr;
		}

		const char* get_variable_name(RE::hkbBehaviorGraph* graph, int32_t ind)
		{
			return graph->data->stringData->variableNames[static_cast<uint32_t>(ind)].c_str();
		}
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
