#define NOMINMAX
#include "Esp.h"
#include "../imgui/imgui.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <chrono>

constexpr int BONE_HEAD = 6;  // Head bone ID


struct PrecomputedColors {
    RGB healthGreen[101];  
};

static PrecomputedColors precompColors;

static struct ColorInitializer {
    ColorInitializer() {
        for (int i = 0; i <= 100; i++) {
            float percent = i / 100.0f;
            precompColors.healthGreen[i] = {
                static_cast<int>(255 * (1.0f - percent)),
                static_cast<int>(255 * percent),
                0,
                255
            };
        }
    }
} colorInit;

EntityCache::EntityCache(UINT refresh_rate_numerator, UINT refresh_rate_denominator, int server_tick_rate)
    : is_updating(false), last_update(std::chrono::steady_clock::now()), update_thread(nullptr) {


    double monitor_hz = (refresh_rate_denominator != 0) ?
        static_cast<double>(refresh_rate_numerator) / refresh_rate_denominator : 100.0;

    double server_tick_interval_ms = 1000.0 / server_tick_rate;

    double target_interval_ms = 1.0;


    update_interval_ms = static_cast<int>(target_interval_ms);

    update_interval_ms = std::clamp(update_interval_ms, 1, 5);

    // Debug için bilgi
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
        "[CS2-External] made by phoneix - Update Interval: %dms (Target FPS: %.0f, Monitor: %.1fHz, Server: %.1fms)\n",
        update_interval_ms, 1000.0 / update_interval_ms, monitor_hz, server_tick_interval_ms);
    OutputDebugStringA(buffer);
}

EntityCache::~EntityCache() {
    is_updating = false;
    std::lock_guard<std::mutex> lock(mtx);
    cached_entities.clear();
    if (update_thread && update_thread->joinable()) {
        update_thread->join();
        delete update_thread;
    }
}

Vector3 EntityCache::GetBonePosition(uintptr_t pawn, int boneId) {
    uintptr_t gameSceneNode = mem::ReadMem<uintptr_t>(pawn + m_pGameSceneNode);
    if (!gameSceneNode) return Vector3{};

    uintptr_t boneMatrix = mem::ReadMem<uintptr_t>(gameSceneNode + m_modelState + 0x80);
    if (!boneMatrix) return Vector3{};

    Vector3 bonePos = mem::ReadMem<Vector3>(boneMatrix + boneId * 32);
    return bonePos;
}

const std::vector<EntityData>& EntityCache::GetEntities(uintptr_t client, const GameData& game_data) {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);

  
    if (elapsed.count() >= update_interval_ms && !is_updating.exchange(true)) {
        if (update_thread && update_thread->joinable()) {
            update_thread->join();
            delete update_thread;
        }

        update_thread = new std::thread([this, client,
            localPlayerPawn = game_data.localPlayerPawn,
            entList = game_data.entList,
            localTeam = game_data.localTeam]() {
                auto new_entities = ReadEntities(client, localPlayerPawn, entList, localTeam);
                std::lock_guard<std::mutex> lock(mtx);
                cached_entities = std::move(new_entities);
                last_update = std::chrono::steady_clock::now();
                is_updating = false;
            });
    }

    std::lock_guard<std::mutex> lock(mtx);
    return cached_entities;
}

std::vector<EntityData> EntityCache::ReadEntities(uintptr_t client, uintptr_t localPlayerPawn,
    uintptr_t entList, int localTeam) {

    std::vector<EntityData> results;
    results.reserve(96);

    for (int i = 0; i < 48; ++i) {
        EntityData data{};
        data.valid = false;

        const uintptr_t listEntry = mem::ReadMem<uintptr_t>(entList + 0x8 * ((i & 0x7FFF) >> 9) + 0x10);
        if (!listEntry) continue;

        const uintptr_t controller = mem::ReadMem<uintptr_t>(listEntry + 0x70 * (i & 0x1FF));
        if (!controller) continue;

        const uintptr_t pawnHandle = mem::ReadMem<uintptr_t>(controller + m_hPawn);
        if (!pawnHandle) continue;

        const uintptr_t pawnEntry = mem::ReadMem<uintptr_t>(entList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 0x10);
        if (!pawnEntry) continue;

        const uintptr_t pawn = mem::ReadMem<uintptr_t>(pawnEntry + 0x70 * (pawnHandle & 0x1FF));
        if (!pawn || pawn == localPlayerPawn) continue;

        data.team = mem::ReadMem<int>(pawn + m_iTeamNum);
        if (data.team == localTeam) continue;

        data.health = mem::ReadMem<int>(pawn + m_iHealth);
        if (data.health <= 0 || data.health > 200) continue;

        data.origin = mem::ReadMem<Vector3>(pawn + m_vOldOrigin);

        data.headBone = GetBonePosition(pawn, BONE_HEAD);
        data.hasBoneData = (data.headBone.x != 0 || data.headBone.y != 0 || data.headBone.z != 0);

        if (data.hasBoneData) {
            data.head = data.headBone;
        }
        else {
            data.head = { data.origin.x, data.origin.y, data.origin.z + 75.0f };
        }

        data.name = mem::ReadString(controller + m_iszPlayerName);
        data.valid = true;

        results.push_back(data);
    }

    return results;
}

namespace render {
    void DrawRect(int x, int y, int w, int h, RGB color, float thickness) {
        auto drawList = ImGui::GetBackgroundDrawList();
        if (drawList) {
            drawList->AddRect(
                ImVec2(static_cast<float>(x), static_cast<float>(y)),
                ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)),
                IM_COL32(color.R, color.G, color.B, color.A),
                0.0f,
                0,
                thickness
            );
        }
    }

    void DrawFilledRect(int x, int y, int w, int h, RGB color) {
        auto drawList = ImGui::GetBackgroundDrawList();
        if (drawList) {
            drawList->AddRectFilled(
                ImVec2(static_cast<float>(x), static_cast<float>(y)),
                ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)),
                IM_COL32(color.R, color.G, color.B, color.A)
            );
        }
    }

    void DrawLine(float x1, float y1, float x2, float y2, RGB color, float thickness) {
        auto drawList = ImGui::GetBackgroundDrawList();
        if (drawList) {
            drawList->AddLine(
                ImVec2(x1, y1),
                ImVec2(x2, y2),
                IM_COL32(color.R, color.G, color.B, color.A),
                thickness
            );
        }
    }

    void DrawText(int x, int y, RGB color, const char* text) {
        auto drawList = ImGui::GetBackgroundDrawList();
        if (drawList) {
            drawList->AddText(
                ImVec2(static_cast<float>(x), static_cast<float>(y)),
                IM_COL32(color.R, color.G, color.B, color.A),
                text
            );
        }
    }

    void DrawCircle(int x, int y, int radius, RGB color, float thickness, int segments) {
        auto drawList = ImGui::GetBackgroundDrawList();
        if (drawList) {
            drawList->AddCircle(
                ImVec2(static_cast<float>(x), static_cast<float>(y)),
                static_cast<float>(radius),
                IM_COL32(color.R, color.G, color.B, color.A),
                segments,
                thickness
            );
        }
    }

    void DrawFilledCircle(int x, int y, int radius, RGB color, int segments) {
        auto drawList = ImGui::GetBackgroundDrawList();
        if (drawList) {
            drawList->AddCircleFilled(
                ImVec2(static_cast<float>(x), static_cast<float>(y)),
                static_cast<float>(radius),
                IM_COL32(color.R, color.G, color.B, color.A),
                segments
            );
        }
    }

    void DrawESP(const std::vector<EntityData>& entities, const GameData& game_data) {
        auto drawList = ImGui::GetBackgroundDrawList();
        if (!drawList) return;

        for (const auto& entity : entities) {
            if (!entity.valid) continue;

            Vector3 headPos = entity.hasBoneData ? entity.headBone : entity.head;

            Vector3 screenFeetPos = entity.origin.w2s(game_data.view_matrix);
            Vector3 screenHeadPos = headPos.w2s(game_data.view_matrix);

            if (screenHeadPos.z < 0.01f || screenFeetPos.z < 0.01f) continue;

       
            float boxHeight = screenFeetPos.y - screenHeadPos.y;

            if (boxHeight < 5.0f) continue;

    
            float boxWidth = boxHeight / 1.6f;

 
            float left = screenHeadPos.x - (boxWidth / 2);
            float top = screenHeadPos.y - 5;

            if (!entity.name.empty()) {
                ImVec2 textSize = ImGui::CalcTextSize(entity.name.c_str());
                DrawText(
                    static_cast<int>(screenHeadPos.x - (textSize.x / 2)),
                    static_cast<int>(top - textSize.y - 5),
                    white,
                    entity.name.c_str()
                );
            }

       
            RGB boxColor = (entity.team == game_data.localTeam) ? green : red;

         
            DrawRect(
                static_cast<int>(left),
                static_cast<int>(top),
                static_cast<int>(boxWidth),
                static_cast<int>(boxHeight + 5),
                boxColor,
                1.5f
            );

            if (entity.hasBoneData) {
                DrawFilledCircle(
                    static_cast<int>(screenHeadPos.x),
                    static_cast<int>(screenHeadPos.y),
                    1,
                    black,
                    12
                );

                DrawCircle(
                    static_cast<int>(screenHeadPos.x),
                    static_cast<int>(screenHeadPos.y),
                    1.2f,
                    black,
                    1.5f,
                    12
                );
            }
            else {
                DrawFilledCircle(
                    static_cast<int>(screenHeadPos.x),
                    static_cast<int>(screenHeadPos.y),
                    1,
                    black,
                    12
                );
            }

   
            int healthIndex = std::clamp(entity.health, 0, 100);
            RGB healthColor = precompColors.healthGreen[healthIndex];

            DrawFilledRect(
                static_cast<int>(left - 6),
                static_cast<int>(top),
                2,
                static_cast<int>(boxHeight + 5),
                black
            );

            float barHeight = (boxHeight + 5) * (entity.health / 100.0f);
            DrawFilledRect(
                static_cast<int>(left - 6),
                static_cast<int>(top + (boxHeight + 5 - barHeight)),
                2,
                static_cast<int>(barHeight),
                healthColor
            );
        }
    }

    void DrawDebugInfo(const GameData& game_data, size_t entity_count) {
        char buffer[128];

        snprintf(buffer, sizeof(buffer),
            "P:0x%p E:%zu T:%d | Made by phoneix",
            (void*)game_data.localPlayerPawn,
            entity_count,
            game_data.localTeam
        );

        DrawText(10, 10, white, buffer);
    }
}