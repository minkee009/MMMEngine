#include "RenderManager.h"
#include <dxgi.h>
#include <dxgi1_6.h>

#pragma comment(lib, "dxgi.lib")

DEFINE_SINGLETON(MMMEngine::RenderManager)

using namespace Microsoft::WRL;

void MMMEngine::RenderManager::RegisterRenderer(ObjPtr<Renderer> renderer)
{
}

void MMMEngine::RenderManager::UnRegisterRenderer(ObjPtr<Renderer> renderer)
{
}

void MMMEngine::RenderManager::ClearScreen()
{
	if (!m_context)
		return;

	// 필요하면 엔진 설정값으로 빼세요
	const float clearColorBackBuffer[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
	const float clearColorScene[4] = { 0.0f,  0.0f,  0.0f,  1.0f };

	// 1) Scene RT clear (HDR 타겟)
	if (m_sceneRTV)
		m_context->ClearRenderTargetView(m_sceneRTV.Get(), clearColorScene);

	// 2) Depth/Stencil clear
	if (m_depthStencilView)
		m_context->ClearDepthStencilView(
			m_depthStencilView.Get(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,   // depth
			0       // stencil
		);

	// 3) BackBuffer clear (최종 프레임 버퍼)
	if (m_renderTargetView)
		m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColorBackBuffer);

	// (선택) 기본 렌더 상태 세팅 (초기화 겸)
	// 블렌드 / 뎁스 상태는 Render()에서 세팅해도 되지만,
	// 일단 Clear에서 기본값 걸어두면 디버그가 편합니다.
	{
		float blendFactor[4] = { 0,0,0,0 };
		m_context->OMSetBlendState(m_blendStateOpaque.Get(), blendFactor, 0xffffffff);
		m_context->OMSetDepthStencilState(m_depthStencilStateLess.Get(), 0);
	}
}

void MMMEngine::RenderManager::ResizeRTVs(int width, int height)
{
	if (!m_device || !m_context || !m_swapChain)
		return;

	// 최소 방어
	if (width <= 0 || height <= 0)
		return;

	HRESULT hr = S_OK;

	// 1) 파이프라인에서 기존 RTV/DSV 바인딩 해제 (ResizeBuffers 전에 필수)
	ID3D11RenderTargetView* nullRTV[1] = { nullptr };
	m_context->OMSetRenderTargets(1, nullRTV, nullptr);
	m_context->Flush();

	// 2) 기존 리소스 릴리즈 (ComPtr면 Reset)
	m_renderTargetView.Reset();

	m_sceneSRV.Reset();
	m_sceneRTV.Reset();
	m_sceneTexture.Reset();

	m_depthStencilView.Reset();
	m_depthStencilTexture.Reset();

	// 3) 스왑체인 버퍼 리사이즈
	//    포맷은 DXGI_FORMAT_UNKNOWN을 주면 기존 포맷 유지
	//    BufferCount는 0을 주면 기존 유지
	hr = m_swapChain->ResizeBuffers(
		0,
		static_cast<UINT>(width),
		static_cast<UINT>(height),
		DXGI_FORMAT_UNKNOWN,
		0
	);
	if (FAILED(hr))
		return;

	// 4) 백버퍼 RTV 재생성
	{
		ComPtr<ID3D11Texture2D> backBuffer;
		hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
		if (FAILED(hr))
			return;

		hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf());
		if (FAILED(hr))
			return;
	}

	// 5) 씬 텍스처/RTV/SRV 재생성 (StartUp과 동일 스펙, 크기만 변경)
	{
		D3D11_TEXTURE2D_DESC descTex = {};
		descTex.Width = static_cast<UINT>(width);
		descTex.Height = static_cast<UINT>(height);
		descTex.MipLevels = 1;
		descTex.ArraySize = 1;
		descTex.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		descTex.SampleDesc.Count = 1;
		descTex.SampleDesc.Quality = 0;
		descTex.Usage = D3D11_USAGE_DEFAULT;
		descTex.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		descTex.CPUAccessFlags = 0;
		descTex.MiscFlags = 0;

		hr = m_device->CreateTexture2D(&descTex, nullptr, m_sceneTexture.GetAddressOf());
		if (FAILED(hr))
			return;

		hr = m_device->CreateRenderTargetView(m_sceneTexture.Get(), nullptr, m_sceneRTV.GetAddressOf());
		if (FAILED(hr))
			return;

		hr = m_device->CreateShaderResourceView(m_sceneTexture.Get(), nullptr, m_sceneSRV.GetAddressOf());
		if (FAILED(hr))
			return;
	}

	// 6) Depth 텍스처/DSV 재생성 (StartUp과 동일 스펙, 크기만 변경)
	{
		D3D11_TEXTURE2D_DESC descDepth = {};
		descDepth.Width = static_cast<UINT>(width);
		descDepth.Height = static_cast<UINT>(height);
		descDepth.MipLevels = 1;
		descDepth.ArraySize = 1;
		descDepth.Format = DXGI_FORMAT_R24G8_TYPELESS;
		descDepth.SampleDesc.Count = 1;
		descDepth.SampleDesc.Quality = 0;
		descDepth.Usage = D3D11_USAGE_DEFAULT;
		descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		descDepth.CPUAccessFlags = 0;
		descDepth.MiscFlags = 0;

		hr = m_device->CreateTexture2D(&descDepth, nullptr, m_depthStencilTexture.GetAddressOf());
		if (FAILED(hr))
			return;

		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
		descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Texture2D.MipSlice = 0;

		hr = m_device->CreateDepthStencilView(m_depthStencilTexture.Get(), &descDSV, m_depthStencilView.GetAddressOf());
		if (FAILED(hr))
			return;
	}
}

void MMMEngine::RenderManager::StartUp(HWND hWnd, int width, int height)
{
	m_commandBuffer.reserve(2048);
	m_screenWidth = width;
	m_screenHeight = height;

	HRESULT hr = S_OK;

	UINT createDeviceFlags = 0;

#ifdef _DEBUG
	// 디버그용 디바이스 플래그 설정
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif //_DEBUG

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		m_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(nullptr, m_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, m_device.GetAddressOf(), &m_featureLevel, m_context.GetAddressOf());

		if (hr == E_INVALIDARG)
		{
			// DirectX 11.0 플랫폼은 D3D_FEATURE_LEVEL_11_1를 인식하지 못하기 때문에 없이 한번 더 시도
			hr = D3D11CreateDevice(nullptr, m_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, m_device.GetAddressOf(), &m_featureLevel, m_context.GetAddressOf());
		}

		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return;

	// DXGI 팩토리를 디바이스에서 부터 얻기
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = m_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return;

	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	if (dxgiFactory2)
	{
		// DirectX 11.1 이거나 이후 버전인 경우
		hr = m_device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(m_device1.GetAddressOf()));
		if (SUCCEEDED(hr))
		{
			ID3D11DeviceContext1* pContext1 = nullptr;
			hr = m_context->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&pContext1));
			if (SUCCEEDED(hr))
			{
				m_context.Attach(pContext1);
			}
		}

		DXGI_SWAP_CHAIN_DESC1 sd = {};
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.BufferCount = 2;

		hr = dxgiFactory2->CreateSwapChainForHwnd(m_device.Get(), hWnd, &sd, nullptr, nullptr, m_swapChain1.GetAddressOf());
		if (SUCCEEDED(hr))
		{
			hr = m_swapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(m_swapChain.GetAddressOf()));
		}

		dxgiFactory2->Release();
	}
	else
	{
		// DirectX 11.0 시스템인 경우
		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 2;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChain(m_device.Get(), &sd, m_swapChain.GetAddressOf());
	}

	dxgiFactory->Release();

	if (!m_dxgiDevice)
	{
		ComPtr<IDXGIDevice> dxgiDevice;
		if (SUCCEEDED(m_device.As(&dxgiDevice)))
		{
			dxgiDevice.As(&m_dxgiDevice); // IDXGIDevice3로 업캐스트
		}
	}

	if (FAILED(hr))
		return;

	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
		return;

	hr = m_device->CreateRenderTargetView(pBackBuffer, nullptr, m_renderTargetView.GetAddressOf());
	pBackBuffer->Release();
	if (FAILED(hr))
		return;

	// 씬 텍스쳐 생성
	D3D11_TEXTURE2D_DESC descTex;
	ZeroMemory(&descTex, sizeof(descTex));
	descTex.Width = width;
	descTex.Height = height;
	descTex.MipLevels = 1;
	descTex.ArraySize = 1;
	descTex.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	descTex.SampleDesc.Count = 1;
	descTex.SampleDesc.Quality = 0;
	descTex.Usage = D3D11_USAGE_DEFAULT;
	descTex.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	descTex.CPUAccessFlags = 0;
	descTex.MiscFlags = 0;
	hr = m_device->CreateTexture2D(&descTex, NULL, m_sceneTexture.GetAddressOf());
	if (FAILED(hr))
		return;

	// 씬 텍스쳐 렌더 타겟 뷰 생성
	hr = m_device->CreateRenderTargetView(m_sceneTexture.Get(), NULL, m_sceneRTV.GetAddressOf());
	if (FAILED(hr))
		return;

	// 씬 텍스쳐 쉐이더 리소스 뷰 생성
	hr = m_device->CreateShaderResourceView(m_sceneTexture.Get(), NULL, m_sceneSRV.GetAddressOf());
	if (FAILED(hr))
		return;

	// 뎁스 스텐실 텍스쳐 생성
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_R24G8_TYPELESS;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = m_device->CreateTexture2D(&descDepth, NULL, m_depthStencilTexture.GetAddressOf());
	if (FAILED(hr))
		return;

	// 뎁스 스텐실 뷰 생성
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = m_device->CreateDepthStencilView(m_depthStencilTexture.Get(), &descDSV, m_depthStencilView.GetAddressOf());
	if (FAILED(hr))
		return;

	// 1) Opaque BlendState (블렌딩 OFF)
	{
		D3D11_BLEND_DESC bd = {};
		bd.AlphaToCoverageEnable = FALSE;
		bd.IndependentBlendEnable = FALSE;

		D3D11_RENDER_TARGET_BLEND_DESC rt = {};
		rt.BlendEnable = FALSE;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		bd.RenderTarget[0] = rt;

		hr = m_device->CreateBlendState(&bd, m_blendStateOpaque.GetAddressOf());
		if (FAILED(hr))
			return;
	}

	// 2) Alpha BlendState
	{
		D3D11_BLEND_DESC bd = {};
		bd.AlphaToCoverageEnable = FALSE;
		bd.IndependentBlendEnable = FALSE;

		D3D11_RENDER_TARGET_BLEND_DESC rt = {};
		rt.BlendEnable = TRUE;

		// Color: out = src*srcA + dst*(1-srcA)
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;

		// Alpha: outA = srcA*1 + dstA*(1-srcA)
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;

		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		bd.RenderTarget[0] = rt;

		hr = m_device->CreateBlendState(&bd, m_blendStateAlpha.GetAddressOf());
		if (FAILED(hr))
			return;
	}

	// 3) DepthStencilState (Depth Less, Write ON)
	{
		D3D11_DEPTH_STENCIL_DESC dsd = {};
		dsd.DepthEnable = TRUE;
		dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsd.DepthFunc = D3D11_COMPARISON_LESS;

		// 스텐실은 기본적으로 OFF
		dsd.StencilEnable = FALSE;
		dsd.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		dsd.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

		hr = m_device->CreateDepthStencilState(&dsd, m_depthStencilStateLess.GetAddressOf());
		if (FAILED(hr))
			return;
	}

	// 4) SamplerState (Linear + Clamp)
	{
		D3D11_SAMPLER_DESC sd = {};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

		sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

		sd.MipLODBias = 0.0f;
		sd.MaxAnisotropy = 1;
		sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

		sd.BorderColor[0] = 0.0f;
		sd.BorderColor[1] = 0.0f;
		sd.BorderColor[2] = 0.0f;
		sd.BorderColor[3] = 0.0f;

		sd.MinLOD = 0.0f;
		sd.MaxLOD = D3D11_FLOAT32_MAX;

		hr = m_device->CreateSamplerState(&sd, m_samplerLinear.GetAddressOf());
		if (FAILED(hr))
			return;
	}
}

void MMMEngine::RenderManager::ResizeScreen(int width, int height)
{
	m_screenWidth = width;
	m_screenHeight = height;
	ResizeRTVs(width, height);
}

void MMMEngine::RenderManager::ShutDown()
{
	if (m_context)
	{
		ID3D11RenderTargetView* nullRTV[8] = { nullptr };
		m_context->OMSetRenderTargets(8, nullRTV, nullptr);

		ID3D11ShaderResourceView* nullSRV[16] = { nullptr };
		m_context->PSSetShaderResources(0, 16, nullSRV);
		m_context->VSSetShaderResources(0, 16, nullSRV);
		m_context->GSSetShaderResources(0, 16, nullSRV);
		m_context->CSSetShaderResources(0, 16, nullSRV);

		m_context->ClearState();
		m_context->Flush();
	}

	// 1) 렌더 타겟/뎁스 및 관련 SRV/RTV (GPU 리소스부터)
	m_postProcessSRV = nullptr;
	m_postProcessRTV = nullptr;

	m_sceneSRV = nullptr;
	m_sceneRTV = nullptr;
	m_sceneTexture = nullptr;

	m_depthStencilView = nullptr;
	m_depthStencilTexture = nullptr;

	m_renderTargetView = nullptr;

	// 2) 상태 객체 (디바이스에 종속)
	m_samplerLinear = nullptr;
	m_depthStencilStateLess = nullptr;
	m_blendStateOpaque = nullptr;
	m_blendStateAlpha = nullptr;

	// 3) 스왑체인/디바이스 관련 (스왑체인 먼저 정리)
	m_swapChain1 = nullptr;
	m_swapChain = nullptr;

	m_dxgiDevice = nullptr;

	// 4) 컨텍스트 -> 디바이스 순서
	m_context = nullptr;

	// (디버그 레이어로 릭 체크를 하려면 디바이스를 마지막에 두는 게 좋음)
	m_device1 = nullptr;
	m_device = nullptr;
}

void MMMEngine::RenderManager::BeginFrame()
{
	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
	vp.Width = (FLOAT)m_screenWidth; vp.Height = (FLOAT)m_screenHeight;
	vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
	m_context->RSSetViewports(1, &vp);

	ClearScreen();
}

void MMMEngine::RenderManager::Render()
{
	m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
}

void MMMEngine::RenderManager::EndFrame()
{
	m_swapChain->Present(m_syncInterval, 0);
}
