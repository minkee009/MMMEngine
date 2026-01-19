#include "RenderManager.h"
#include "GameObject.h"
#include "Transform.h"
#include <EditorCamera.h>
#include <RendererTools.h>
#include "VShader.h"
#include "PShader.h"
#include "ResourceManager.h"

DEFINE_SINGLETON(MMMEngine::RenderManager)

using namespace Microsoft::WRL;

namespace MMMEngine {
    void RenderManager::Initialize(HWND* _hwnd, UINT _ClientWidth, UINT _ClientHeight)
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
        m_pHwnd = _hwnd;

        // 클라이언트 사이즈 등록
        m_rClientWidth = _ClientWidth;
        m_rClientHeight = _ClientHeight;

		// 카메라 생성
		// WARNING::오브젝트 생성!!
		// TODO::에디터 구현부로 이 코드 옮기기
		auto camera = ObjectManager::Get().NewObject<GameObject>("EditorCamera");
		m_pCamera = camera->AddComponent<EditorCamera>();

        // 인스턴스 초기화 뭉탱이
        this->InitD3D();
        this->Start();
    }
    void RenderManager::InitD3D()
    {
        // 스왑체인 속성설정 생성
        DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
        swapDesc.BufferCount = 1;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.Width = m_rClientWidth;
        swapDesc.Height = m_rClientHeight;
        swapDesc.SampleDesc.Count = 1;		// MSAA
        swapDesc.SampleDesc.Quality = 0;	// MSAA 품질수준

		UINT creationFlag = 0;

		// 팩토리 생성
		Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
		HR_T(CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)dxgiFactory.GetAddressOf()));

		// 스왑체인 생성
		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
		HR_T(dxgiFactory->CreateSwapChainForHwnd(m_pDevice.Get(), *m_pHwnd, &swapDesc,
			nullptr, nullptr, swapChain.GetAddressOf()));
		HR_T(swapChain.As(&m_pSwapChain));

		// 컨텍스트 생성
		ComPtr<ID3D11DeviceContext3> context;
		m_pDevice->GetImmediateContext3(context.GetAddressOf());
		HR_T(context.As(&m_pDeviceContext));

		// 렌더타겟 생성
		HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D1), (void**)m_pBackBuffer.GetAddressOf()));
		HR_T(m_pDevice->CreateRenderTargetView1(m_pBackBuffer.Get(), nullptr, m_pRenderTargetView.GetAddressOf()));
		HR_T(m_pDevice->CreateShaderResourceView1(m_pBackBuffer.Get(), nullptr, m_pBackSRV.GetAddressOf()));

		// 뷰포트 설정
		m_defaultViewport = {};
		m_defaultViewport.TopLeftX = 0.0f;
		m_defaultViewport.TopLeftY = 0.0f;
		m_defaultViewport.Width = static_cast<float>(m_rClientWidth);
		m_defaultViewport.Height = static_cast<float>(m_rClientHeight);
		m_defaultViewport.MinDepth = 0.0f;
		m_defaultViewport.MaxDepth = 1.0f;

		// 뎊스 텍스쳐 생성
		D3D11_TEXTURE2D_DESC1 depthDesc = {};
		depthDesc.Width = m_rClientWidth;
		depthDesc.Height = m_rClientHeight;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthDesc.CPUAccessFlags = 0;
		depthDesc.MiscFlags = 0;

		ComPtr<ID3D11Texture2D1> depthTexture;
		HR_T(m_pDevice->CreateTexture2D1(&depthDesc, nullptr, depthTexture.GetAddressOf()));

		// 뎊스스탠실 뷰 생성
		D3D11_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = depthDesc.Format;
		dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		HR_T(m_pDevice->CreateDepthStencilView(depthTexture.Get(), &dsv, m_pDepthStencilView.GetAddressOf()));

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
    }
    void RenderManager::UnInitD3D()
    {
    }
    void RenderManager::Start()
    {
        // 버퍼 기본색상
        m_ClearColor = DirectX::SimpleMath::Vector4(0.45f, 0.55f, 0.60f, 1.00f);

        m_pCamera->GetViewMatrix(m_camMat.mView);
        m_camMat.mProjection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, m_rClientWidth / (FLOAT)m_rClientHeight, 0.01f, 100.0f);

        // 캠 버퍼 생성
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = 0;

        bd.ByteWidth = sizeof(Render_CamBuffer);
        HR_T(m_pDevice->CreateBuffer(&bd, nullptr, m_pCambuffer.GetAddressOf()));

		// 기본 VSShader 생성 (VS 쉐이더는 매크로 넣지않는 이상 스킨드매쉬 쉐이더가 아님)
		m_pDefaultVSShader = ResourceManager::Get().Load<VShader>(L"Shader/PBR/VS/SkeletalVertexShader.hlsl");
		m_pDefaultPSShader = ResourceManager::Get().Load<PShader>(L"Shader/PBR/PS/BRDFShader.hlsl");
	}

	void RenderManager::BeginFrame()
	{
		// Clear
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), m_backColor);
		m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	void RenderManager::Render()
    {
		// 캠 버퍼 업데이트
		auto camTrans = m_pCamera->GetTransform();
		m_camMat.camPos = (DirectX::SimpleMath::Vector4)camTrans->GetWorldPosition();
		m_pCamera->GetViewMatrix(m_camMat.mView);
		m_camMat.mView = DirectX::XMMatrixTranspose(m_camMat.mView);
		m_camMat.mProjection = DirectX::XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, m_rClientWidth / (FLOAT)m_rClientHeight, 0.01f, 100.0f));

		// 리소스 업데이트
		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());

		m_pDeviceContext->RSSetViewports(1, &m_defaultViewport);
		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());
		m_pDeviceContext->OMSetRenderTargets(1, reinterpret_cast<ID3D11RenderTargetView* const*>(m_pRenderTargetView.GetAddressOf()), m_pDepthStencilView.Get());

		// RenderPass
		for (const auto& pass : m_Passes) {
			for (const auto& renderer : pass.second) {
				renderer->Render();
			}
		}
    }

	void RenderManager::EndFrame()
	{
		// Present our back buffer to our front buffer
		m_pSwapChain->Present(0, 0);
	}

}
