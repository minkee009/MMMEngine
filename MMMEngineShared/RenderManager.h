#pragma once
#include "ExportSingleton.hpp"
#include <algorithm>
#define NOMINMAX
#include <memory>
#include <unordered_map>
#include <vector>
#include <dxgi1_3.h>

//#include <directxtk/CommonStates.h>
//#include <directxtk/Effects.h>

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl/client.h> // Microsoft::WRL::ComPtr

#include "Renderer.h"
#include "RenderCommand.h"



#pragma comment(lib, "d3d11.lib")

namespace MMMEngine
{
	class MMMENGINE_API RenderManager : public Utility::ExportSingleton<RenderManager>
	{
	private:
        friend class Renderer;
        int m_screenWidth = 0;
        int m_screenHeight = 0;
        int m_syncInterval = 1;

        // 초기화용
        D3D_DRIVER_TYPE m_driverType = D3D_DRIVER_TYPE_NULL;
        D3D_FEATURE_LEVEL m_featureLevel = D3D_FEATURE_LEVEL_11_0;

        // 핵심 인프라
        Microsoft::WRL::ComPtr<ID3D11Device>            m_device;
        Microsoft::WRL::ComPtr<ID3D11Device1>           m_device1;
        Microsoft::WRL::ComPtr<IDXGIDevice3>            m_dxgiDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext>     m_context;
        Microsoft::WRL::ComPtr<IDXGISwapChain>          m_swapChain;
        Microsoft::WRL::ComPtr<IDXGISwapChain1>         m_swapChain1;

        // 렌더 타겟 및 뎁스 버퍼
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_renderTargetView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_depthStencilTexture;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_depthStencilView;

        // 공용 스테이트 (초기화 시 미리 생성)
        Microsoft::WRL::ComPtr<ID3D11BlendState>        m_blendStateAlpha;
        Microsoft::WRL::ComPtr<ID3D11BlendState>        m_blendStateOpaque;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilStateLess;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_samplerLinear;

        // 씬을 처음에 그릴 중간 버퍼 (HDR 처리를 위해 보통 float 포맷 사용)
        Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_sceneTexture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_sceneRTV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_sceneSRV;

        // 필요하다면 핑퐁(Ping-pong) 버퍼 (블룸, 블러 등 다단계 후처리 시)
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_postProcessRTV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_postProcessSRV;

        std::vector<ObjPtr<Renderer>> m_renderers;
        void RegisterRenderer(ObjPtr<Renderer> renderer);
        void UnRegisterRenderer(ObjPtr<Renderer> renderer);

        std::vector<RenderCommand> m_commandBuffer;

        //void SortCommands() {
        //    std::sort(m_commandBuffer.begin(), m_commandBuffer.end(),
        //        [](const RenderCommand& a, const RenderCommand& b) {
        //            // 1. 큐 순서 (Shadow -> Skybox -> Opaque -> ...)
        //            if (a.queue != b.queue) return a.queue < b.queue;

        //            // 2. 사용자 지정 순서
        //            if (a.renderPriority != b.renderPriority) return a.renderPriority < b.renderPriority;

        //            // 3. 큐 종류에 따른 세부 정렬
        //            if (a.queue == RenderQueue::Transparent || a.queue == RenderQueue::Particle) {
        //                // 투명 객체: 뒤에서부터 (Distance 내림차순)
        //                return a.distanceSq > b.distanceSq;
        //            }

        //            else {
        //                // 불투명 객체: 앞에서부터 (Early-Z 최적화)
        //                if (std::abs(a.distanceSq - b.distanceSq) > 0.0001f)
        //                    return a.distanceSq < b.distanceSq;

        //                // 4. 거리까지 같다면 머터리얼끼리 모아서 상태 변경 최적화
        //                return a.materialID < b.materialID;
        //            }
        //        });
        //}
        void ClearScreen();
        void ResizeRTVs(int width, int height);
	public:
        // 리소스 생성 도우미 (Renderer 컴포넌트들이 사용)
        ID3D11Device* GetDevice() { return m_device.Get(); }
        ID3D11DeviceContext* GetContext() { return m_context.Get(); }

        void SetSyncInterval(int interval) { m_syncInterval = interval; }

        void StartUp(HWND hWnd, int width, int height);
        void ResizeScreen(int width, int height);
        void ShutDown();

        // 렌더링 루프 진입점
        void BeginFrame();
        void Render();
        void EndFrame(); // Present() 호출
	};
}