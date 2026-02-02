#include "GameViewWindow.h"
#include "EditorRegistry.h"
#include "RenderManager.h"
#include "UIEventManager.h"

using namespace MMMEngine::EditorRegistry;
using namespace MMMEngine::Editor;
using namespace MMMEngine;

// GameViewWindow.cpp
void MMMEngine::Editor::GameViewWindow::Render()
{
    if (!g_editor_window_gameView)
        return;

    ImGuiWindowClass wc;
    wc.ParentViewportId = ImGui::GetMainViewport()->ID;
    wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
    ImGui::SetNextWindowClass(&wc);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_None;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin(u8"\uf11b 게임", &g_editor_window_gameView))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    UINT resX = 0, resY = 0;
    RenderManager::Get().GetSceneSize(resX, resY);

    ImVec2 contentRegion = ImGui::GetContentRegionAvail();

    if (resX > 0 && resY > 0 && contentRegion.x > 0.0f && contentRegion.y > 0.0f)
    {
        float targetAspect = (float)resX / (float)resY;

        ImVec2 displaySize;
        float windowAspect = contentRegion.x / contentRegion.y;

        if (windowAspect > targetAspect) {
            displaySize.y = contentRegion.y;
            displaySize.x = contentRegion.y * targetAspect;
        }
        else {
            displaySize.x = contentRegion.x;
            displaySize.y = contentRegion.x / targetAspect;
        }

        float offsetX = (contentRegion.x - displaySize.x) * 0.5f;
        float offsetY = (contentRegion.y - displaySize.y) * 0.5f;
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY));

        auto sceneSRV = RenderManager::Get().GetSceneSRV();
        if (sceneSRV.Get())
        {
            ImGui::Image((ImTextureID)sceneSRV.Get(), displaySize);

            ImVec2 itemMin = ImGui::GetItemRectMin();
            ImVec2 itemMax = ImGui::GetItemRectMax();
            ImVec2 mousePos = ImGui::GetMousePos();
            bool hovered = ImGui::IsItemHovered();
            bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

            static bool capture = false;
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                capture = true;
            if (!mouseDown)
                capture = false;

            bool canInteract = EditorRegistry::g_editor_scene_playing;
            bool active = canInteract && (hovered || capture);
            if (active)
            {
                if (mousePos.x < itemMin.x) mousePos.x = itemMin.x;
                if (mousePos.y < itemMin.y) mousePos.y = itemMin.y;
                if (mousePos.x > itemMax.x) mousePos.x = itemMax.x;
                if (mousePos.y > itemMax.y) mousePos.y = itemMax.y;

                const float sizeX = itemMax.x - itemMin.x;
                const float sizeY = itemMax.y - itemMin.y;
                if (sizeX > 0.0f && sizeY > 0.0f)
                {
                    const float u = (mousePos.x - itemMin.x) / sizeX;
                    const float v = (mousePos.y - itemMin.y) / sizeY;
                    DirectX::SimpleMath::Vector2 scenePos{ u * resX, v * resY };
                    UIEventManager::Get().UpdateFromScenePointer(scenePos, mouseDown, true);
                }
                else
                {
                    UIEventManager::Get().UpdateFromScenePointer({}, false, false);
                }
            }
            else
            {
                UIEventManager::Get().UpdateFromScenePointer({}, false, false);
            }
        }
        else
        {
            ImGui::TextUnformatted("Scene SRV is null.");
            UIEventManager::Get().UpdateFromScenePointer({}, false, false);
        }
    }
    else
    {
        ImGui::TextUnformatted("Invalid scene size or content region.");
        UIEventManager::Get().UpdateFromScenePointer({}, false, false);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

