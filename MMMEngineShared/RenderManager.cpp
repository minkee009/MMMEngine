#include "RenderManager.h"

#include "RendererTools.h"
#include "RenderShared.h"
#include "ShaderInfo.h"
#include "ResourceManager.h"
#include "GameObject.h"
#include "Transform.h"
#include "Camera.h"
#include "Renderer.h"
#include "Material.h"

#include "rttr/registration.h"
#include <cmath>

DEFINE_SINGLETON(MMMEngine::RenderManager)

using namespace Microsoft::WRL;
using namespace DirectX::SimpleMath;
using namespace DirectX;

RTTR_REGISTRATION{
	using namespace rttr;
	using namespace MMMEngine;
	
	rttr::registration::class_<RenderManager>("RenderManager")
		.property("maincamera", &RenderManager::GetCamera, &RenderManager::SetCamera);
}

namespace MMMEngine {

	RenderManager::RenderManager()
	{
		m_worldMatrix = Matrix::Identity;
		m_viewMatrix = Matrix::Identity;
		m_projMatrix = Matrix::Identity;
	}

	void RenderManager::ApplyMatToContext(ID3D11DeviceContext4* _context, Material* _material)
	{
		if (!_material->GetPShader())
			return;

		auto VS = _material->GetVShader();
		auto PS = _material->GetPShader();
		ShaderType type = ShaderInfo::Get().GetShaderType(PS->GetFilePath());
		_context->VSSetShader(VS->m_pVShader.Get(), nullptr, 0);
		_context->PSSetShader(PS->m_pPShader.Get(), nullptr, 0);

		// TODO::인풋레이아웃 ShaderInfo 사용해 자동등록 시키기
		_context->IASetInputLayout(_material->GetVShader()->m_pInputLayout.Get());

		// TODO::샘플러 ShaderInfo 사용해 자동등록화 시키기 (UpdateProperty 사용, 프로퍼티로 샘플러 관리하기)
		_context->PSSetSamplers(0, 1, m_pDafaultSampler.GetAddressOf());
		_context->PSSetSamplers(1, 1, m_pCompareSampler.GetAddressOf());


		// 메테리얼
		for (auto& [prop, val] : _material->GetProperties()) {
			UpdateProperty(prop, val, type);
		}
	}

	void RenderManager::ExcuteCommands()
	{
		for (auto& [type, commands] : m_renderCommands)
		{
			if (type == RenderType::R_TRANSCULANT)
			{
				// 투명 오브젝트: 카메라 거리 내림차순 정렬
				std::sort(commands.begin(), commands.end(),
					[](const RenderCommand& a, const RenderCommand& b)
					{
						return a.camDistance > b.camDistance;
					});
			}
			else if (type == RenderType::R_SKYBOX)
			{
				if (m_pSkyboxMaterial.expired()) {
					m_pSkyboxMaterial = commands[0].material;

					if(m_pSkyboxMaterial.expired())
						continue;

					// 공용 리소스 등록
					for (auto& [prop, val] : m_pSkyboxMaterial.lock()->GetProperties())
						ShaderInfo::Get().AddGlobalPropVal(S_PBR, prop, val);
				}
			}
			else
			{
				// 불투명 오브젝트: 머티리얼 기준 정렬
				std::sort(commands.begin(), commands.end(),
					[](const RenderCommand& a, const RenderCommand& b)
					{
						if (a.material.expired() || b.material.expired())
							return false;
						return a.material.lock() < b.material.lock();
					});
			}

			// 정렬된 커맨드 실행
			std::weak_ptr<Material> lastMaterial;
			for (auto& cmd : commands)
			{
				if (cmd.material.expired())
					continue;

				auto lMat = lastMaterial.lock();
				auto cMat = cmd.material.lock();

				if (cMat != lMat)
				{
					ApplyMatToContext(m_pDeviceContext.Get(), cMat.get());
					lastMaterial = cmd.material;
					lMat = cMat;
				}


				UINT stride = sizeof(Mesh_Vertex); // 실제 버텍스 구조체 크기
				UINT offset = 0;
				m_pDeviceContext->IASetVertexBuffers(0, 1, &cmd.vertexBuffer, &stride, &offset);
				m_pDeviceContext->IASetIndexBuffer(cmd.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

				if (cmd.boneMatIndex >= 0)
				{
					// 스킨드 메시라면 본 인덱스를 셰이더에 전달
					// UpdateBoneIndexConstantBuffer(cmd.boneMatIndex);
				}

				// 상수버퍼 등록
				auto sType = ShaderInfo::Get().GetShaderType(lMat->GetPShader()->GetFilePath());

				// 상수버퍼 일렬업데이트
				ShaderInfo::Get().UpdateCBuffers(sType);

				// 월드매트릭스 버퍼집어넣기
				Render_TransformBuffer transformBuffer;
				transformBuffer.mWorld = XMMatrixTranspose(m_objWorldMatMap[cmd.worldMatIndex]);
				transformBuffer.mNormalMatrix = XMMatrixInverse(nullptr, m_objWorldMatMap[cmd.worldMatIndex]);
				m_pDeviceContext->UpdateSubresource1(m_pTransbuffer.Get(), 0, nullptr, &transformBuffer, 0, 0, D3D11_COPY_DISCARD);
				m_pDeviceContext->VSSetConstantBuffers(1, 1, m_pTransbuffer.GetAddressOf());


				if (type == RenderType::R_SKYBOX)
					m_pDeviceContext->Draw(3, 0);
				else
					m_pDeviceContext->DrawIndexed(cmd.indiciesSize, 0, 0);
			}
		}
	}

	void RenderManager::InitCache()
	{
		// 캐싱 컨테이너 초기화
		m_objWorldMatMap.clear();
		m_renderCommands.clear();
		m_rObjIdx = 0;
	}

	void RenderManager::InitRenderers()
	{
		int size = static_cast<int>(m_renInitQueue.size());
		for (int i = 0; i < size; ++i) {
			auto renderer = m_renInitQueue.front();
			m_renInitQueue.pop();

			if (!renderer->IsActiveAndEnabled())
				m_renInitQueue.push(renderer);
			else
				renderer->Init();
		}
	}

	void RenderManager::UpdateRenderers()
	{
		for (auto& renderer : m_renderers) {
			if (renderer->IsActiveAndEnabled()) {
				renderer->Render();
			}
		}
	}

	void RenderManager::UpdateLights()
	{
		for (auto& light : m_lights) {
			if (light->IsActiveAndEnabled()) {
				light->Render();
			}
		}
	}

	void RenderManager::StartUp(HWND _hwnd, UINT _ClientWidth, UINT _ClientHeight)
	{
		// 디바이스 생성
		ComPtr<ID3D11Device> device;
		D3D_FEATURE_LEVEL featureLevel;
		D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
			0, nullptr, 0, D3D11_SDK_VERSION,
			device.GetAddressOf(), &featureLevel, nullptr);

		HR_T(device.As(&m_pDevice));

		// hWnd 등록
		assert(_hwnd != nullptr && "RenderPipe::Initialize : hWnd must not be nullptr!!");
		m_hWnd = _hwnd;

		// 클라이언트 사이즈 등록
		m_clientWidth = _ClientWidth;
		m_clientHeight = _ClientHeight;

		// 인스턴스 초기화 뭉탱이
		InitD3D();
		Start();
	}
	void RenderManager::InitD3D()
	{
		// 스왑체인 속성설정 생성
		DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
		swapDesc.BufferCount = 2;
		swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapDesc.Width = m_clientWidth;
		swapDesc.Height = m_clientHeight;
		swapDesc.SampleDesc.Count = 1;		// MSAA
		swapDesc.SampleDesc.Quality = 0;	// MSAA 품질수준

		// DXGI 디바이스
		ComPtr<IDXGIDevice> dxgiDevice;
		m_pDevice.As(&dxgiDevice);

		ComPtr<IDXGIAdapter> adapter;
		dxgiDevice->GetAdapter(&adapter);

		// 팩토리 생성
		ComPtr<IDXGIFactory2> dxgiFactory;
		adapter->GetParent(IID_PPV_ARGS(&dxgiFactory));

		// 스왑체인 생성
		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
		HR_T(dxgiFactory->CreateSwapChainForHwnd(m_pDevice.Get(), m_hWnd, &swapDesc,
			nullptr, nullptr, swapChain.GetAddressOf()));
		HR_T(swapChain.As(&m_pSwapChain));

		// 컨텍스트 생성
		ComPtr<ID3D11DeviceContext3> context;
		m_pDevice->GetImmediateContext3(context.GetAddressOf());
		HR_T(context.As(&m_pDeviceContext));

		// 스왑체인 렌더타겟 생성
		ID3D11Texture2D1* backBuffer;
		HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D1), (void**)&backBuffer));
		HR_T(m_pDevice->CreateRenderTargetView1(backBuffer, nullptr, m_pRenderTargetView.GetAddressOf()));
		backBuffer->Release();

		// 뷰포트 설정
		m_swapViewport = {};
		m_swapViewport.TopLeftX = 0.0f;
		m_swapViewport.TopLeftY = 0.0f;
		m_swapViewport.Width = static_cast<float>(m_clientWidth);
		m_swapViewport.Height = static_cast<float>(m_clientHeight);
		m_swapViewport.MinDepth = 0.0f;
		m_swapViewport.MaxDepth = 1.0f;

		// 뎊스 텍스쳐 생성
		D3D11_TEXTURE2D_DESC1 depthDesc = {};
		depthDesc.Width = m_clientWidth;
		depthDesc.Height = m_clientHeight;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthDesc.CPUAccessFlags = 0;
		depthDesc.MiscFlags = 0;

		HR_T(m_pDevice->CreateTexture2D1(&depthDesc, nullptr, m_pDepthStencilBuffer.GetAddressOf()));

		// 뎊스스탠실 뷰 생성
		D3D11_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = depthDesc.Format;
		dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		HR_T(m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), &dsv, m_pDepthStencilView.GetAddressOf()));

		// 래스터라이저 속성 생성
		D3D11_RASTERIZER_DESC2 defaultRsDesc = {};
		defaultRsDesc.FillMode = D3D11_FILL_SOLID;
		defaultRsDesc.CullMode = D3D11_CULL_BACK;
		defaultRsDesc.FrontCounterClockwise = FALSE;
		defaultRsDesc.DepthBias = 0;
		defaultRsDesc.DepthBiasClamp = 0.0f;
		defaultRsDesc.SlopeScaledDepthBias = 0.0f;
		defaultRsDesc.DepthClipEnable = TRUE;
		defaultRsDesc.ScissorEnable = FALSE;
		defaultRsDesc.MultisampleEnable = FALSE;
		defaultRsDesc.AntialiasedLineEnable = FALSE;
		HR_T(m_pDevice->CreateRasterizerState2(&defaultRsDesc, m_pDefaultRS.GetAddressOf()));

		// 블랜드 스테이트 생성
		D3D11_BLEND_DESC1 blendDesc = {};
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;
		blendDesc.RenderTarget[0].BlendEnable = FALSE;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		HR_T(m_pDevice->CreateBlendState1(&blendDesc, m_pDefaultBS.GetAddressOf()));
		assert(m_pDefaultBS && "RenderPipe::InitD3D : defaultBS not initialized!!");

		// 레스터라이저 스테이트 생성
		D3D11_RASTERIZER_DESC2 rsDesc = {};
		rsDesc.FillMode = D3D11_FILL_SOLID;
		rsDesc.CullMode = D3D11_CULL_BACK;
		rsDesc.FrontCounterClockwise = FALSE;
		rsDesc.DepthBias = 0;
		rsDesc.DepthBiasClamp = 0.0f;
		rsDesc.SlopeScaledDepthBias = 0.0f;
		rsDesc.DepthClipEnable = TRUE;
		rsDesc.ScissorEnable = FALSE;
		rsDesc.MultisampleEnable = FALSE;
		rsDesc.AntialiasedLineEnable = FALSE;
		HR_T(m_pDevice->CreateRasterizerState2(&rsDesc, m_pDefaultRS.GetAddressOf()));
		assert(m_pDefaultRS && "RenderPipe::InitD3D : defaultRS not initialized!!");
	
		// 샘플러 만들기
		D3D11_SAMPLER_DESC sampDesc = {};
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

		HR_T(m_pDevice->CreateSamplerState(&sampDesc, m_pDafaultSampler.GetAddressOf()));
		
		sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; // 비교 필터
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; // 깊이 비교 함수
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

		HR_T(m_pDevice->CreateSamplerState(&sampDesc, m_pCompareSampler.GetAddressOf()));

		// === Scene 렌더타겟 초기화 ===
		D3D11_TEXTURE2D_DESC1 sceneColorDesc = {};
		sceneColorDesc.Width = m_clientWidth;
		sceneColorDesc.Height = m_clientHeight;
		sceneColorDesc.MipLevels = 1;
		sceneColorDesc.ArraySize = 1;
		sceneColorDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR 지원 포맷
		sceneColorDesc.SampleDesc.Count = 1;
		sceneColorDesc.SampleDesc.Quality = 0;
		sceneColorDesc.Usage = D3D11_USAGE_DEFAULT;
		sceneColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		// Scene 컬러 텍스처 생성
		HR_T(m_pDevice->CreateTexture2D1(&sceneColorDesc, nullptr, m_pSceneTexture.GetAddressOf()));

		// RTV 생성
		HR_T(m_pDevice->CreateRenderTargetView1(m_pSceneTexture.Get(), nullptr, m_pSceneRTV.GetAddressOf()));

		// SRV 생성
		HR_T(m_pDevice->CreateShaderResourceView1(m_pSceneTexture.Get(), nullptr, m_pSceneSRV.GetAddressOf()));

		// Depth/Stencil 버퍼 생성
		D3D11_TEXTURE2D_DESC1 sceneDepthDesc = {};
		sceneDepthDesc.Width = m_clientWidth;
		sceneDepthDesc.Height = m_clientHeight;
		sceneDepthDesc.MipLevels = 1;
		sceneDepthDesc.ArraySize = 1;
		sceneDepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		sceneDepthDesc.SampleDesc.Count = 1;
		sceneDepthDesc.SampleDesc.Quality = 0;
		sceneDepthDesc.Usage = D3D11_USAGE_DEFAULT;
		sceneDepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		sceneDepthDesc.CPUAccessFlags = 0;
		sceneDepthDesc.MiscFlags = 0;

		HR_T(m_pDevice->CreateTexture2D1(&sceneDepthDesc, nullptr, m_pSceneDSB.GetAddressOf()));

		// DSV 생성
		HR_T(m_pDevice->CreateDepthStencilView(m_pSceneDSB.Get(), nullptr, m_pSceneDSV.GetAddressOf()));

		m_sceneWidth = m_clientWidth;
		m_sceneHeight = m_clientHeight;

		// 씬 뷰포트 설정
		m_sceneViewport.TopLeftX = 0.0f;
		m_sceneViewport.TopLeftY = 0.0f;
		m_sceneViewport.Width = static_cast<float>(m_sceneWidth);
		m_sceneViewport.Height = static_cast<float>(m_sceneHeight);
		m_sceneViewport.MinDepth = 0.0f;
		m_sceneViewport.MaxDepth = 1.0f;

		// 캠 버퍼 생성
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;

		bd.ByteWidth = sizeof(Render_CamBuffer);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, m_pCambuffer.GetAddressOf()));
		bd.ByteWidth = sizeof(Render_TransformBuffer);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pTransbuffer));
		bd.ByteWidth = sizeof(Render_ShadowBuffer);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pShadowBuffer));

		// 그림자 버퍼용
		D3D11_TEXTURE2D_DESC1 shadowDesc = {};
		shadowDesc.Width = m_shadowMapWidth;
		shadowDesc.Height = m_shadowMapHeight;
		shadowDesc.MipLevels = 1;
		shadowDesc.ArraySize = 1;
		shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		shadowDesc.SampleDesc.Count = 1;
		shadowDesc.SampleDesc.Quality = 0;
		shadowDesc.Usage = D3D11_USAGE_DEFAULT;
		shadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		shadowDesc.CPUAccessFlags = 0;
		shadowDesc.MiscFlags = 0;

		// 텍스처 생성
		HR_T(m_pDevice->CreateTexture2D1(&shadowDesc, nullptr, m_pShadowTexture.GetAddressOf()));

		// DepthStencilView
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;

		HR_T(m_pDevice->CreateDepthStencilView(m_pShadowTexture.Get(), &dsvDesc, m_pShadowDSV.GetAddressOf()));

		// ShaderResourceView
		m_pShadowSRV = std::make_shared<Texture2D>();
		D3D11_SHADER_RESOURCE_VIEW_DESC1 srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		HR_T(m_pDevice->CreateShaderResourceView1(m_pShadowTexture.Get(), &srvDesc, m_pShadowSRV->m_pSRV.GetAddressOf()));

		// ShadwoViewport
		m_shadowVP.TopLeftX = 0.0f;
		m_shadowVP.TopLeftY = 0.0f;
		m_shadowVP.Width = (FLOAT)m_shadowMapWidth;
		m_shadowVP.Height = (FLOAT)m_shadowMapHeight;
		m_shadowVP.MinDepth = 0.0f;
		m_shadowVP.MaxDepth = 1.0f;
	}
	void RenderManager::ShutDown()
	{
		// COM객체 초기화
		m_pDevice->Release();
		m_pDeviceContext->Release();
		m_pSwapChain->Release();


		//// 변수 초기화
		//while (!m_initQueue.empty())
		//	m_initQueue.pop();

		m_worldMatrix = Matrix::Identity;
		m_viewMatrix = Matrix::Identity;
		m_projMatrix = Matrix::Identity;
	}
	void RenderManager::Start()
	{
		// 버퍼 기본색상
		m_ClearColor = DirectX::SimpleMath::Vector4(0.45f, 0.55f, 0.60f, 1.00f);
	}

	void RenderManager::UpdateProperty(const std::wstring& _propName, const PropertyValue& _value, ShaderType _type)
	{
		std::visit([&](auto&& arg)
			{
				using T = std::decay_t<decltype(arg)>;

				if constexpr (std::is_same_v<T, int> ||
					std::is_same_v<T, float> ||
					std::is_same_v<T, DirectX::SimpleMath::Vector3> ||
					std::is_same_v<T, DirectX::SimpleMath::Vector4> ||
					std::is_same_v<T, DirectX::SimpleMath::Matrix>)
				{
					ShaderInfo::Get().UpdateProperty(m_pDeviceContext.Get(), _type, _propName, &arg);
				}
				else if constexpr (std::is_same_v<T, ResPtr<MMMEngine::Texture2D>>)
				{
					if (arg == nullptr)
						return;

					ID3D11ShaderResourceView* srv = arg->m_pSRV.Get();
					ShaderInfo::Get().UpdateProperty(m_pDeviceContext.Get(), _type, _propName, srv);
				}
			}, _value);
	}

	void RenderManager::SetWorldMatrix(DirectX::SimpleMath::Matrix& _world)
	{
		m_worldMatrix = _world;
	}

	void RenderManager::SetViewMatrix(DirectX::SimpleMath::Matrix& _view)
	{
		m_viewMatrix = _view;
	}

	void RenderManager::SetProjMatrix(DirectX::SimpleMath::Matrix& _proj)
	{
		m_projMatrix = _proj;
	}

	void RenderManager::ResizeSwapChainSize(int width, int height)
	{
		m_clientWidth = width;
		m_clientHeight = height;

		// RTV 등록해제
		m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

		// 기존 RTV/DSV 해제
		if (m_pRenderTargetView) m_pRenderTargetView->Release();
		if (m_pDepthStencilView) m_pDepthStencilView->Release();
		if (m_pDepthStencilBuffer) m_pDepthStencilBuffer->Release();

		// ResizeBuffers 호출
		HR_T(m_pSwapChain->ResizeBuffers(
			0,
			static_cast<UINT>(width),
			static_cast<UINT>(height),
			DXGI_FORMAT_UNKNOWN,
			0
		));

		// 새 백버퍼 가져오기
		ID3D11Texture2D1* buffer;
		HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D1), (void**)&buffer));

		// 새 RTV 생성
		HR_T(m_pDevice->CreateRenderTargetView1(buffer, nullptr, m_pRenderTargetView.GetAddressOf()));
		buffer->Release();

		// Depth/Stencil 버퍼 생성
		D3D11_TEXTURE2D_DESC1 depthDesc = {};
		depthDesc.Width = width;
		depthDesc.Height = height;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		HR_T(m_pDevice->CreateTexture2D1(&depthDesc, nullptr, m_pDepthStencilBuffer.GetAddressOf()));
		HR_T(m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), nullptr, m_pDepthStencilView.GetAddressOf()));
	}

	void RenderManager::ResizeSceneSize(int _sceneWidth, int _sceneHeight)
	{
		m_sceneWidth = _sceneWidth;
		m_sceneHeight = _sceneHeight;

		// 기존 리소스 해제
		if (m_pSceneRTV) { m_pSceneRTV->Release(); }
		if (m_pSceneTexture) { m_pSceneTexture->Release(); }
		if (m_pSceneSRV) { m_pSceneSRV->Release(); }
		if (m_pSceneDSV) { m_pSceneDSV->Release(); }
		if (m_pSceneDSB) { m_pSceneDSB->Release(); }

		// 컬러 텍스처 설명
		D3D11_TEXTURE2D_DESC1 colorDesc = {};
		colorDesc.Width = _sceneWidth;
		colorDesc.Height = _sceneHeight;
		colorDesc.MipLevels = 1;
		colorDesc.ArraySize = 1;
		colorDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR 지원 포맷
		colorDesc.SampleDesc.Count = 1;
		colorDesc.SampleDesc.Quality = 0;
		colorDesc.Usage = D3D11_USAGE_DEFAULT;
		colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		// Scene 컬러 텍스처 생성
		HR_T(m_pDevice->CreateTexture2D1(&colorDesc, nullptr, m_pSceneTexture.GetAddressOf()));

		// RTV 생성
		HR_T(m_pDevice->CreateRenderTargetView1(m_pSceneTexture.Get(), nullptr, m_pSceneRTV.GetAddressOf()));

		// SRV 생성 (쉐이더에서 샘플링 가능)
		HR_T(m_pDevice->CreateShaderResourceView1(m_pSceneTexture.Get(), nullptr, m_pSceneSRV.GetAddressOf()));

		// Depth/Stencil 버퍼 설명
		D3D11_TEXTURE2D_DESC1 depthDesc = {};
		depthDesc.Width = _sceneWidth;
		depthDesc.Height = _sceneHeight;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		// Depth/Stencil 텍스처 생성
		HR_T(m_pDevice->CreateTexture2D1(&depthDesc, nullptr, m_pSceneDSB.GetAddressOf()));

		// DSV 생성
		HR_T(m_pDevice->CreateDepthStencilView(m_pSceneDSB.Get(), nullptr, m_pSceneDSV.GetAddressOf()));
		

		// 뷰포트 갱신
		m_sceneViewport.Width = static_cast<float>(_sceneWidth);
		m_sceneViewport.Height = static_cast<float>(_sceneHeight);
		m_sceneViewport.MinDepth = 0.0f;
		m_sceneViewport.MaxDepth = 1.0f;
		m_sceneViewport.TopLeftX = 0.0f;
		m_sceneViewport.TopLeftY = 0.0f;

		// todo : 렌더러 작업자에게 꼭 고지하기
		// 카메라 Aspect Ratio 변경
		if (m_pMainCamera.IsValid())
		{
			m_pMainCamera->SetAspect(static_cast<float>(_sceneWidth) / static_cast<float>(_sceneHeight));
		}
	}

	void RenderManager::AddCommand(RenderType _type, RenderCommand&& _command)
	{
		m_renderCommands[_type].push_back(std::move(_command));
	}

	int RenderManager::AddMatrix(const DirectX::SimpleMath::Matrix& _worldMatrix)
	{
		int index = m_rObjIdx++;
		m_objWorldMatMap[index] = _worldMatrix;

		return index;
	}

	void RenderManager::ClearAllCommands()
	{
		m_renderCommands.clear();
	}

	void RenderManager::BeginFrame()
	{
		// Clear
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), m_backColor);
		m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// TODO :: 글로벌 쉐이더인포 삭제하기 (라이트는 관리했는데 스카이박스 데이터는 관리안함 바꾸셈)
		ShaderInfo::Get().ClearWorldPropertyDatas();

		// 렌더러 컨트롤
		InitRenderers();
		UpdateRenderers();
		UpdateLights();

		// 카메라 유효성 확인
		//if(!m_pMainCamera.IsValid())
		//	m_pMainCamera = Camera::GetMainCamera();
		//if (!m_pMainCamera.IsValid()) {
		//	m_pMainCamera = Camera::CreateMainCamera()->GetComponent<Camera>();
		//}
	}

	void RenderManager::ShadowRender()
	{
		bool flag = false;
		for (auto& light : m_lights) {
			if (light->IsActiveAndEnabled()) {
				flag = true;
				break;
			}
		}

		if (!flag)
			return;

		// 버퍼데이터 생성
		Render_ShadowBuffer shadowBuffer;

		// 라이트 방향 (정규화)
		DirectX::SimpleMath::Vector3 lightDir = DirectX::SimpleMath::Vector3::Zero;

		// 글로벌 프로퍼티 찾기
		for (int i = 0; i < static_cast<int>(ShaderType::S_END); ++i) {
			ShaderType type = static_cast<ShaderType>(i);
			auto& propval = ShaderInfo::Get().GetGlobalPropVal(type, L"mLightDir");

			if (auto p = std::get_if<DirectX::SimpleMath::Vector3>(&propval)) {
				lightDir = *p;
				break;
			}
		}
		lightDir.Normalize();

		// 라이트 정보
		DirectX::XMMATRIX invView = XMMatrixInverse(nullptr, m_pMainCamera->GetViewMatrix());
		DirectX::XMVECTOR camPos = invView.r[3];
		DirectX::SimpleMath::Vector3 target = camPos;
		
		DirectX::SimpleMath::Vector3 lightPos = camPos;
		auto offset = (-lightDir * 500.0f);
		lightPos += offset;

		DirectX::SimpleMath::Vector3 up{ 0.0f, 1.0f, 0.0f };

		shadowBuffer.ShadowView = XMMatrixTranspose(DirectX::XMMatrixLookAtLH(lightPos, target, up));

		// 직교 투영 행렬 (쉐도우맵 범위 설정)
		float orthoWidth = 128.0f;   // 그림자 범위 (씬 크기에 맞게 조정)
		float orthoHeight = 128.0f;
		float nearZ = 0.1f;
		float farZ = 1000.0f;

		shadowBuffer.ShadowProjection =
			XMMatrixTranspose(
				XMMatrixOrthographicLH(
					orthoWidth,
					orthoHeight,
					nearZ,
					farZ
				)
			);

		m_lightPos = lightPos;
		m_lightView = shadowBuffer.ShadowView;
		m_lightProj = shadowBuffer.ShadowProjection;

		// -- 렌더 설정 --
		// 캠버퍼 업데이트
		Render_CamBuffer m_camMat;
		m_camMat.mView = m_lightView;
		m_camMat.mProjection = m_lightProj;
		m_camMat.camPos = { m_lightPos.x, m_lightPos.y, m_lightPos.z , 1.0f };
		m_camMat.mInvProjection = m_lightProj.Invert();

		// RTV는 nullptr, DSV만 설정
		m_pDeviceContext->OMSetRenderTargets(0, nullptr, m_pShadowDSV.Get());

		// 깊이 버퍼 클리어
		m_pDeviceContext->ClearDepthStencilView(m_pShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		float blendFactor[4] = { 0,0,0,0 };
		UINT sampleMask = 0xffffffff;
		m_pDeviceContext->OMSetBlendState(m_pDefaultBS.Get(), blendFactor, sampleMask);

		m_pDeviceContext->RSSetViewports(1, &m_shadowVP);
		m_pDeviceContext->PSSetSamplers(0, 1, m_pDafaultSampler.GetAddressOf());
		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());

		// 리소스 업데이트
		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);
		m_pDeviceContext->UpdateSubresource1(m_pShadowBuffer.Get(), 0, nullptr, &shadowBuffer, 0, 0, D3D11_COPY_DISCARD);

		// 셰이더에 바인딩
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->VSSetConstantBuffers(4, 1, m_pShadowBuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());

		for (auto& [type, commands] : m_renderCommands)
		{
			if (type == RenderType::R_SHADOWMAP ||
				type == RenderType::R_PREDEPTH ||
				type == RenderType::R_SKYBOX ||
				type == RenderType::R_PARTICLE ||
				type == RenderType::R_POSTPROCESS ||
				type == RenderType::R_UI ||
				type == RenderType::R_NONE ||
				type == RenderType::R_END)
			{
				// 쉐도우 안그리는거는 스킵
				continue;
			}
			
			if (type == RenderType::R_TRANSCULANT)
			{
				// 투명 오브젝트: 카메라 거리 내림차순 정렬
				std::sort(commands.begin(), commands.end(),
					[](const RenderCommand& a, const RenderCommand& b)
					{
						return a.camDistance > b.camDistance;
					});
			}
			else
			{
				// 불투명 오브젝트: 머티리얼 기준 정렬
				std::sort(commands.begin(), commands.end(),
					[](const RenderCommand& a, const RenderCommand& b)
					{
						if (a.material.expired() || b.material.expired())
							return false;
						return a.material.lock() < b.material.lock();
					});
			}

			// 정렬된 커맨드 실행
			std::weak_ptr<Material> lastMaterial;
			for (auto& cmd : commands)
			{
				if (cmd.material.expired())
					continue;

				auto lMat = lastMaterial.lock();
				auto cMat = cmd.material.lock();

				if (cMat != lMat)
				{
					auto VS = cMat->m_pVShader;
					auto PS = ShaderInfo::Get().GetShadowPShader();

					m_pDeviceContext->VSSetShader(VS->m_pVShader.Get(), nullptr, 0);
					m_pDeviceContext->PSSetShader(PS->m_pPShader.Get(), nullptr, 0);

					// 자동등록 시키기
					m_pDeviceContext->IASetInputLayout(VS->m_pInputLayout.Get());

					// Albedo 등록
					auto tex2D = std::get_if<ResPtr<Texture2D>>(&cMat->GetProperty(L"_albedo"));
					if ((*tex2D)) {
						ID3D11ShaderResourceView* albedo = (*tex2D)->m_pSRV.Get();
						m_pDeviceContext->PSSetShaderResources(0, 1, &albedo);
					}

					lastMaterial = cmd.material;
					lMat = cMat;
				}

				UINT stride = sizeof(Mesh_Vertex); // 실제 버텍스 구조체 크기
				UINT offset = 0;
				m_pDeviceContext->IASetVertexBuffers(0, 1, &cmd.vertexBuffer, &stride, &offset);
				m_pDeviceContext->IASetIndexBuffer(cmd.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

				if (cmd.boneMatIndex >= 0)
				{
					// 스킨드 메시라면 본 인덱스를 셰이더에 전달
					// UpdateBoneIndexConstantBuffer(cmd.boneMatIndex);
				}

				// 월드매트릭스 버퍼집어넣기
				Render_TransformBuffer transformBuffer;
				transformBuffer.mWorld = XMMatrixTranspose(m_objWorldMatMap[cmd.worldMatIndex]);
				transformBuffer.mNormalMatrix = XMMatrixInverse(nullptr, m_objWorldMatMap[cmd.worldMatIndex]);
				m_pDeviceContext->UpdateSubresource1(m_pTransbuffer.Get(), 0, nullptr, &transformBuffer, 0, 0, D3D11_COPY_DISCARD);
				m_pDeviceContext->VSSetConstantBuffers(1, 1, m_pTransbuffer.GetAddressOf());

				m_pDeviceContext->DrawIndexed(cmd.indiciesSize, 0, 0);
			}
		}
		
		// 글로벌 쉐도우맵 추가
		ShaderInfo::Get().AddAllGlobalPropVal(L"_shadowmap", m_pShadowSRV);
	}

	void RenderManager::Render()
	{
		if (!m_pMainCamera.IsValid())
		{
			// Clear
			m_pDeviceContext->ClearRenderTargetView(m_pSceneRTV.Get(), m_backColor);
			m_pDeviceContext->ClearDepthStencilView(m_pSceneDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			m_pDeviceContext->OMSetRenderTargets(1, reinterpret_cast<ID3D11RenderTargetView* const*>(m_pRenderTargetView.GetAddressOf()), nullptr);
			return;
		}

		// Clear
		m_pDeviceContext->ClearRenderTargetView(m_pSceneRTV.Get(), m_backColor);
		m_pDeviceContext->ClearDepthStencilView(m_pSceneDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// 그림자 렌더링
		ShadowRender();

		// 캠 버퍼 업데이트
		Render_CamBuffer m_camMat = {};
		m_camMat.mView = XMMatrixTranspose(m_pMainCamera->GetViewMatrix());
		m_camMat.mProjection = XMMatrixTranspose(m_pMainCamera->GetProjMatrix());
		m_camMat.camPos = XMMatrixInverse(nullptr, m_pMainCamera->GetViewMatrix()).r[3];
		m_camMat.mInvProjection = XMMatrixTranspose(m_pMainCamera->GetProjMatrix().Invert());

		// 리소스 업데이트
		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		float blendFactor[4] = { 0,0,0,0 };
		UINT sampleMask = 0xffffffff;
		m_pDeviceContext->OMSetBlendState(m_pDefaultBS.Get(), blendFactor, sampleMask);

		m_pDeviceContext->RSSetViewports(1, &m_sceneViewport);
		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());
		m_pDeviceContext->OMSetRenderTargets(1, reinterpret_cast<ID3D11RenderTargetView* const*>(m_pSceneRTV.GetAddressOf()), m_pSceneDSV.Get());

		// 렌더커맨드 소팅, 실행
		ExcuteCommands();
		
		// 씬렌더 해제
		m_pDeviceContext->RSSetViewports(1, &m_swapViewport);
		m_pDeviceContext->OMSetRenderTargets(1, reinterpret_cast<ID3D11RenderTargetView* const*>(m_pRenderTargetView.GetAddressOf()), nullptr);

		// (풀스크린 트라이앵글)
		if (useBackBuffer) {
			auto& vs = ShaderInfo::Get().GetFullScreenVShader();
			auto& ps = ShaderInfo::Get().GetFullScreenPShader();
			m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			m_pDeviceContext->IASetInputLayout(nullptr);
			m_pDeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
			m_pDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
			m_pDeviceContext->VSSetShader(vs->m_pVShader.Get(), nullptr, 0);
			m_pDeviceContext->PSSetShader(ps->m_pPShader.Get(), nullptr, 0);

			ID3D11ShaderResourceView* sceneSRV = m_pSceneSRV.Get();
			m_pDeviceContext->PSSetShaderResources(0, 1, &sceneSRV);
			m_pDeviceContext->PSSetSamplers(0, 1, m_pDafaultSampler.GetAddressOf());
			m_pDeviceContext->PSSetSamplers(1, 1, m_pCompareSampler.GetAddressOf());

			// 씬 뷰포트 설정
			float sceneAspect = static_cast<float>(m_sceneWidth) / static_cast<float>(m_sceneHeight);
			float swapchainAspect = static_cast<float>(m_clientWidth) / static_cast<float>(m_clientHeight);

			float drawWf, drawHf;

			if (swapchainAspect > sceneAspect) {
				drawHf = static_cast<float>(m_clientHeight);
				drawWf = static_cast<float>(m_clientHeight) * sceneAspect;
			}
			else {
				drawWf = static_cast<float>(m_clientWidth);
				drawHf = static_cast<float>(m_clientWidth) / sceneAspect;
			}

			// 정수 픽셀 기준으로 스냅
			int drawW = static_cast<int>(std::round(drawWf));
			int drawH = static_cast<int>(std::round(drawHf));
			int offsetX = (static_cast<int>(m_clientWidth) - drawW) / 2;
			int offsetY = (static_cast<int>(m_clientHeight) - drawH) / 2;

			m_swapViewport.TopLeftX = static_cast<float>(offsetX);
			m_swapViewport.TopLeftY = static_cast<float>(offsetY);
			m_swapViewport.Width = static_cast<float>(drawW);
			m_swapViewport.Height = static_cast<float>(drawH);
			m_swapViewport.MinDepth = 0.0f;
			m_swapViewport.MaxDepth = 1.0f;

			// 변경된 뷰포트를 실제 파이프라인에 반영
			m_pDeviceContext->RSSetViewports(1, &m_swapViewport);

			m_pDeviceContext->Draw(3, 0);
		}
	}

	void RenderManager::RenderOnlyRenderer()
	{
		// 캠 버퍼 업데이트
		Render_CamBuffer m_camMat = {};
		m_camMat.camPos = XMMatrixInverse(nullptr, m_viewMatrix).r[3];
		m_camMat.mView = XMMatrixTranspose(m_viewMatrix);
		m_camMat.mProjection = XMMatrixTranspose(m_projMatrix);
		m_camMat.mInvProjection = XMMatrixTranspose(m_projMatrix.Invert());

		// 리소스 업데이트
		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		// ID 만드는 

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		float blendFactor[4] = { 0,0,0,0 };
		UINT sampleMask = 0xffffffff;
		m_pDeviceContext->OMSetBlendState(m_pDefaultBS.Get(), blendFactor, sampleMask);

		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());

		// RenderPass
		ExcuteCommands();
	}

	void RenderManager::RenderPickingIds(ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11InputLayout* layout, ID3D11Buffer* idBuffer)
	{
		if (!vs || !ps || !layout || !idBuffer)
			return;

		// 캠 버퍼 업데이트
		Render_CamBuffer m_camMat = {};
		m_camMat.camPos = XMMatrixInverse(nullptr, m_viewMatrix).r[3];
		m_camMat.mView = XMMatrixTranspose(m_viewMatrix);
		m_camMat.mProjection = XMMatrixTranspose(m_projMatrix);

		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->IASetInputLayout(layout);
		m_pDeviceContext->VSSetShader(vs, nullptr, 0);
		m_pDeviceContext->PSSetShader(ps, nullptr, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->VSSetConstantBuffers(1, 1, m_pTransbuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(5, 1, &idBuffer);
		float blendFactor[4] = { 0,0,0,0 };
		UINT sampleMask = 0xffffffff;
		m_pDeviceContext->OMSetBlendState(m_pDefaultBS.Get(), blendFactor, sampleMask);
		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());
		m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);

		struct PickingIdBuffer
		{
			uint32_t objectId = 0;
			uint32_t padding[3] = { 0, 0, 0 };
		} pickData;

		for (auto& [type, commands] : m_renderCommands)
		{
			if (type == RenderType::R_SKYBOX)
				continue;

			for (auto& cmd : commands)
			{
				if (cmd.rendererID == UINT32_MAX)
					continue;

				UINT stride = sizeof(Mesh_Vertex);
				UINT offset = 0;
				m_pDeviceContext->IASetVertexBuffers(0, 1, &cmd.vertexBuffer, &stride, &offset);
				m_pDeviceContext->IASetIndexBuffer(cmd.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

				Render_TransformBuffer transformBuffer;
				transformBuffer.mWorld = XMMatrixTranspose(m_objWorldMatMap[cmd.worldMatIndex]);
				transformBuffer.mNormalMatrix = XMMatrixInverse(nullptr, m_objWorldMatMap[cmd.worldMatIndex]);
				m_pDeviceContext->UpdateSubresource1(m_pTransbuffer.Get(), 0, nullptr, &transformBuffer, 0, 0, D3D11_COPY_DISCARD);

				pickData.objectId = cmd.rendererID + 1;
				m_pDeviceContext->UpdateSubresource1(idBuffer, 0, nullptr, &pickData, 0, 0, D3D11_COPY_DISCARD);

				m_pDeviceContext->DrawIndexed(cmd.indiciesSize, 0, 0);
			}
		}
	}

	void RenderManager::RenderSelectedMask(ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11InputLayout* layout, const uint32_t* ids, uint32_t count)
	{
		if (!vs || !ps || !layout || !ids || count == 0)
			return;

		Render_CamBuffer m_camMat = {};
		m_camMat.camPos = XMMatrixInverse(nullptr, m_viewMatrix).r[3];
		m_camMat.mView = XMMatrixTranspose(m_viewMatrix);
		m_camMat.mProjection = XMMatrixTranspose(m_projMatrix);

		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->IASetInputLayout(layout);
		m_pDeviceContext->VSSetShader(vs, nullptr, 0);
		m_pDeviceContext->PSSetShader(ps, nullptr, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->VSSetConstantBuffers(1, 1, m_pTransbuffer.GetAddressOf());
		// State (blend/depth/raster) is expected to be set by caller.

		auto isSelected = [ids, count](uint32_t id) -> bool
		{
			for (uint32_t i = 0; i < count; ++i)
			{
				if (ids[i] == id)
					return true;
			}
			return false;
		};

		for (auto& [type, commands] : m_renderCommands)
		{
			if (type == RenderType::R_SKYBOX)
				continue;

			for (auto& cmd : commands)
			{
				if (cmd.rendererID == UINT32_MAX || !isSelected(cmd.rendererID))
					continue;

				UINT stride = sizeof(Mesh_Vertex);
				UINT offset = 0;
				m_pDeviceContext->IASetVertexBuffers(0, 1, &cmd.vertexBuffer, &stride, &offset);
				m_pDeviceContext->IASetIndexBuffer(cmd.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

				Render_TransformBuffer transformBuffer;
				transformBuffer.mWorld = XMMatrixTranspose(m_objWorldMatMap[cmd.worldMatIndex]);
				transformBuffer.mNormalMatrix = XMMatrixInverse(nullptr, m_objWorldMatMap[cmd.worldMatIndex]);
				m_pDeviceContext->UpdateSubresource1(m_pTransbuffer.Get(), 0, nullptr, &transformBuffer, 0, 0, D3D11_COPY_DISCARD);

				m_pDeviceContext->DrawIndexed(cmd.indiciesSize, 0, 0);
			}
		}
	}

	void RenderManager::EndFrame()
	{
		// 캐싱된 데이터들 해제
		InitCache();

		// Present our back buffer to our front buffer
		m_pSwapChain->Present(m_rSyncInterval, 0);
	}

	uint32_t RenderManager::AddRenderer(Renderer* _renderer)
	{
		if (_renderer == nullptr)
			return UINT32_MAX;

		uint32_t id = m_nextRendererId++;
		m_renderers.push_back(_renderer);
		m_rendererIdMap[id] = _renderer;
		m_renInitQueue.push(_renderer);
		return id;
	}

	void RenderManager::RemoveRenderer(int _idx)
	{
		if (m_renderers.empty())
			return;

		auto it = m_rendererIdMap.find(static_cast<uint32_t>(_idx));
		if (it == m_rendererIdMap.end())
			return;

		Renderer* target = it->second;
		m_rendererIdMap.erase(it);

		for (size_t i = 0; i < m_renderers.size(); ++i)
		{
			if (m_renderers[i] == target)
			{
				m_renderers[i] = m_renderers.back();
				m_renderers.pop_back();
				break;
			}
		}
	}

	int RenderManager::AddLight(Light* _obj)
	{
		if (_obj == nullptr)
			return -1;

		int index = static_cast<int>(m_lights.size());
		m_lights.push_back(_obj);

		return index;
	}

	void RenderManager::RemoveLight(int _idx)
	{
		if (m_lights.empty())
			return;

		if (_idx < m_lights.size() && _idx >= 0)
		{
			if (m_lights.size() == 1)
			{
				m_lights.pop_back();
				return;
			}

			std::swap(m_lights[_idx], m_lights.back());
			m_lights[_idx]->m_lightIndex = _idx;
			m_lights.pop_back();
		}
	}

	void RenderManager::SetShadowMapSize(UINT _size)
	{

	}

	Renderer* RenderManager::GetRendererById(uint32_t id) const
	{
		auto it = m_rendererIdMap.find(id);
		if (it == m_rendererIdMap.end())
			return nullptr;
		return it->second;
	}
}
