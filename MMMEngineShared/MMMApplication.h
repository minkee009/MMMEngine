#pragma once
#include "App.h"
#include "GlobalRegistry.h"
#include "RenderManager.h"
#include <cassert>
#include <string>

namespace MMMEngine::Application
{
	inline void Quit()
	{
		assert(GlobalRegistry::g_pApp && "�۷ι� ������Ʈ���� Application�� ��ϵǾ����� �ʽ��ϴ�!");

		MMMEngine::GlobalRegistry::g_quitRequested = true;
		return;
	}

	inline void SetWindowTitle(const std::wstring& title) { assert(GlobalRegistry::g_pApp && "�۷ι� ������Ʈ���� Application�� ��ϵǾ����� �ʽ��ϴ�!"); GlobalRegistry::g_pApp->SetWindowTitle(title); }

	inline void SetWindowSize(const float& width, const float& height) { assert(GlobalRegistry::g_pApp && "�۷ι� ������Ʈ���� Application�� ��ϵǾ����� �ʽ��ϴ�!"); GlobalRegistry::g_pApp->SetWindowSize(width, height); }

	// todo : ���� �Ŵ��� �۾��ڿ��� SyncInterval�� �ʿ��ϴٰ� �����Ұ�
	//inline void SetVSyncInterval(int interval) { RenderManager::Get().SetSyncInterval(interval); }
}
