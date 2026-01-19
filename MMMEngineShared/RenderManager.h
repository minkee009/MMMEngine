#pragma once
#include "Export.h"
#include "ExportSingleton.hpp"
#include <RendererBase.h>
#include <map>
#include <vector>
#include <memory>
#include <type_traits>
#include <typeindex>

#include <dxgi1_4.h>
#include <wrl/client.h>
#include <SimpleMath.h>

#include <RenderShared.h>
#include <Object.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")

namespace MMMEngine
{
	class Transform;
	class EditorCamera;
	class Material;
	class MMMENGINE_API RenderManager : public Utility::ExportSingleton<RenderManager>
	{
		friend class Utility::ExportSingleton<RenderManager>;
		friend class RendererBase;
		friend class Material;
	private:
		RenderManager() = default;
		std::map<int, std::vector<std::shared_ptr<RendererBase>>> m_Passes;

    protected:
        HWND* m_pHwnd = nullptr;	// HWND ъ명
        UINT m_rClientWidth = 0;
        UINT m_rClientHeight = 0;
        float m_backColor[4] = { 0.0f, 0.5f, 0.5f, 1.0f };	// 諛깃렇쇱대 而щ
		
		// 諛댁
		Microsoft::WRL::ComPtr<ID3D11Device5> m_pDevice;

		// 湲곕낯 � 명고댁
		Microsoft::WRL::ComPtr<ID3D11DeviceContext4> m_pDeviceContext;		// 諛댁 而⑦ㅽ
		Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;				// ㅼ泥댁

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView1> m_pRenderTargetView;	// �留 寃酉
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_pDepthStencilView;		// 源닿 泥由щ�  ㅼㅽ 酉

		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_pDafaultSamplerLinear;		//  .
		Microsoft::WRL::ComPtr<ID3D11RasterizerState2> m_pDefaultRS;			// 湲곕낯 RS

		Microsoft::WRL::ComPtr<ID3D11BlendState1> m_pDefaultBS;		// 湲곕낯 釉 ㅽ댄
		Microsoft::WRL::ComPtr<ID3D11RasterizerState2> m_DefaultRS;	// 湲곕낯 �ㅽ곕쇱댁 ㅽ댄
		D3D11_VIEWPORT m_defaultViewport;							// 湲곕낯 酉고ы

		// 踰 湲곕낯
		DirectX::SimpleMath::Vector4 m_ClearColor;
<<<<<<< HEAD
		
		// 명 �댁
		std::shared_ptr<VShader> m_pDefaultVSShader;
		std::shared_ptr<PShader> m_pDefaultPSShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_pInputLayout;
=======
>>>>>>> parent of 417ccbf ([Add] Material Fix, MatSerealizer, ShaderResource)

		// 移대 愿�
		ObjPtr<EditorCamera> m_pCamera;
		Render_CamBuffer m_camMat;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pCambuffer = nullptr;		// 몃ㅽ 踰

		// 諛깅 ㅼ
		Microsoft::WRL::ComPtr<ID3D11Texture2D1> m_pBackBuffer = nullptr;		// 諛깅 ㅼ
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView1> m_pBackSRV = nullptr;	// 諛깅 SRV
    public:
		void Initialize(HWND* _hwnd, UINT _ClientWidth, UINT _ClientHeight);
		void InitD3D();
		void UnInitD3D();
		void Start();
		void Render();

		const Microsoft::WRL::ComPtr<ID3D11Device5> GetDevice() const { return m_pDevice; }
	public:
		template <typename T, typename... Args>
		std::weak_ptr<RendererBase> AddRenderer(RenderType _passType, Args&&... args);

		template <typename T>
		bool RemoveRenderer(RenderType _passType, std::shared_ptr<T>& _renderer);
	};

	template <typename T, typename... Args>
	std::weak_ptr<RendererBase>
		RenderManager::AddRenderer(RenderType _passType, Args&&... args)
	{
		std::shared_ptr<T> temp = std::make_shared<T>(std::forward<Args>(args)...);
		passes[_passType].push_back(temp);

		return temp;
	}

	template <typename T>
	bool RenderManager::RemoveRenderer(RenderType _passType, std::shared_ptr<T>& _renderer)
	{
		if (_renderer && (passes.find(_passType) != passes.end())) {
			auto it = std::find(passes[_passType].begin(), passes[_passType].end(), _renderer);

			if (it != passes[_passType].end()) {
				passes[_passType].erase(it);
				return true;
			}
		}

		return false;
	}
} 
