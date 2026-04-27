#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <atomic>
#include "../mem/memory.h"
#include "../offsets/offsets.h"
#include "../math/math.h"
#include <algorithm>

struct RGB {
    int R, G, B, A;
};

inline RGB green = { 0, 255, 0, 255 };
inline RGB red = { 255, 0, 0, 255 };
inline RGB white = { 255, 255, 255, 255 };
inline RGB black = { 0, 0, 0, 255 };
inline RGB blue = { 0, 0, 255, 255 };
inline RGB yellow = { 255, 255, 0, 255 };
inline RGB cyan = { 0, 255, 255, 255 }; 

struct EntityData {
    Vector3 origin;
    Vector3 head;
    Vector3 headBone;
    int health = 0;
    int team = 0;
    std::string name;
    bool valid = false;
    bool hasBoneData = false;

    explicit operator bool() const { return valid; }
};

struct GameData {
    std::mutex mtx;
    std::uintptr_t localPlayerPawn = 0;
    std::uintptr_t entList = 0;
    view_matrix_t view_matrix;
    int localTeam = 0;
    std::uintptr_t client = 0;
    DWORD pid = 0;

    bool IsValid() const {
        return localPlayerPawn && entList;
    }
};

class EntityCache {
public:
    explicit EntityCache(UINT refresh_rate_numerator = 144, UINT refresh_rate_denominator = 1, int server_tick_rate = 128);
    ~EntityCache();
    const std::vector<EntityData>& GetEntities(uintptr_t client, const GameData& game_data);

private:
    std::vector<EntityData> ReadEntities(uintptr_t client, uintptr_t localPlayerPawn, uintptr_t entList, int localTeam);
    Vector3 GetBonePosition(uintptr_t pawn, int boneId);

    std::vector<EntityData> cached_entities;
    std::mutex mtx;
    std::atomic<bool> is_updating{ false };
    int update_interval_ms;
    std::chrono::steady_clock::time_point last_update;
    std::thread* update_thread = nullptr; 
};

namespace render {
    void DrawRect(int x, int y, int w, int h, RGB color, float thickness);
    void DrawFilledRect(int x, int y, int w, int h, RGB color);
    void DrawLine(float x1, float y1, float x2, float y2, RGB color, float thickness);
    void DrawText(int x, int y, RGB color, const char* text);
    void DrawCircle(int x, int y, int radius, RGB color, float thickness, int segments = 12);
    void DrawFilledCircle(int x, int y, int radius, RGB color, int segments = 12);
    void DrawESP(const std::vector<EntityData>& entities, const GameData& game_data);
    void DrawDebugInfo(const GameData& game_data, size_t entity_count);
}