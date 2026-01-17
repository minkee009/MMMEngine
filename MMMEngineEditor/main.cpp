#define NOMINMAX
#include <iostream>

#include "GlobalRegistry.h"
#include "EditorRegistry.h"
#include "App.h"

#include "InputManager.h"
#include "ResourceManager.h"
#include "TimeManager.h"
#include "RenderManager.h"
#include "BehaviourManager.h"
#include "SceneManager.h"
#include "ObjectManager.h"

#include "ImGuiEditorContext.h"

using namespace MMMEngine;
using namespace MMMEngine::Utility;
using namespace MMMEngine::Editor;

void Initialize()
{
	auto app = GlobalRegistry::g_pApp;
	auto hwnd = app->GetWindowHandle();
	auto windowInfo = app->GetWindowInfo();

	InputManager::Get().StartUp(hwnd);
	TimeManager::Get().StartUp();
	SceneManager::Get().StartUp(L"Assets/Scenes", true);
	app->OnWindowSizeChanged.AddListener<InputManager, &InputManager::HandleWindowResize>(&InputManager::Get());

	ObjectManager::Get().StartUp();
	BehaviourManager::Get().StartUp();

	RenderManager::Get().StartUp(hwnd, windowInfo.width, windowInfo.height);
	app->OnWindowSizeChanged.AddListener<RenderManager, &RenderManager::ResizeScreen>(&RenderManager::Get());

	ImGuiEditorContext::Get().Initialize(hwnd, RenderManager::Get().GetDevice(), RenderManager::Get().GetContext());
	app->OnBeforeWindowMessage.AddListener<ImGuiEditorContext, &ImGuiEditorContext::HandleWindowMessage>(&ImGuiEditorContext::Get());
}

void Update()
{
	TimeManager::Get().BeginFrame();
	InputManager::Get().Update();

	float dt = TimeManager::Get().GetDeltaTime();
	if (SceneManager::Get().CheckSceneIsChanged())
	{
		ObjectManager::Get().UpdateInternalTimer(dt);
		BehaviourManager::Get().DisableBehaviours();
		ObjectManager::Get().ProcessPendingDestroy();
		BehaviourManager::Get().AllSortBehaviours();
		BehaviourManager::Get().AllBroadCastBehaviourMessage("OnSceneLoaded");
	}

	static bool isGameRunning = false;

	if (isGameRunning)
	{
		BehaviourManager::Get().InitializeBehaviours();
	}

	TimeManager::Get().ConsumeFixedSteps([&](float fixedDt)
		{
			if (!isGameRunning)
				return;

			//PhysicsManager::Get()->PreSyncPhysicsWorld();
			//PhysicsManager::Get()->PreApplyTransform();
			BehaviourManager::Get().BroadCastBehaviourMessage("FixedUpdate");
			//PhysicsManager::Get()->Simulate(fixedDt);
			//PhysicsManager::Get()->ApplyTransform();
		});

	RenderManager::Get().BeginFrame();
	RenderManager::Get().Render();
	ImGuiEditorContext::Get().BeginFrame();
	ImGuiEditorContext::Get().Render();
	ImGuiEditorContext::Get().EndFrame();
	RenderManager::Get().EndFrame();

	ObjectManager::Get().UpdateInternalTimer(dt);
	BehaviourManager::Get().DisableBehaviours();
	ObjectManager::Get().ProcessPendingDestroy();
}

void Release()
{
	GlobalRegistry::g_pApp = nullptr;
	ImGuiEditorContext::Get().Uninitialize();
	RenderManager::Get().ShutDown();
	TimeManager::Get().ShutDown();
	InputManager::Get().ShutDown();

	SceneManager::Get().ShutDown();
	ObjectManager::Get().ShutDown();
	BehaviourManager::Get().ShutDown();
}

int main()
{
	App app{ L"MMMEditor",1600,900 };
	GlobalRegistry::g_pApp = &app;

	app.OnInitialize.AddListener<&Initialize>();
	app.OnUpdate.AddListener<&Update>();
	app.OnRelease.AddListener<&Release>();
	app.Run();
}