#pragma once
#include "HavokScript.h"
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

namespace Claude {
    // Config
    void SetAPIKey(const std::string& apiKey);
    void SetModel(const std::string& model);
    void SetMaxTokens(int maxTokens);

    // Synchronous query (blocking)
    std::string Query(const std::string& prompt, const std::string& systemPrompt = "");

    // Turn-based queries (rate limited to one per turn)
    std::string QueryForTurn(int turnNumber, const std::string& prompt, const std::string& systemPrompt = "");
    void QueryForTurnAsync(int turnNumber, const std::string& prompt, const std::string& systemPrompt = "");

    // Asynchronous query (non-blocking, no rate limit)
    void QueryAsync(const std::string& prompt, const std::string& systemPrompt = "");
    bool HasResponse();
    std::string GetResponse();

    // Lua bindings
    int lua_Query(hks::lua_State* L);
    int lua_QueryForTurn(hks::lua_State* L);
    int lua_QueryForTurnAsync(hks::lua_State* L);
    int lua_QueryAsync(hks::lua_State* L);
    int lua_HasResponse(hks::lua_State* L);
    int lua_GetResponse(hks::lua_State* L);
    int lua_SetAPIKey(hks::lua_State* L);
    int lua_SetModel(hks::lua_State* L);
    int lua_SetMaxTokens(hks::lua_State* L);

    // Registration function (called from Hook_RegisterScriptData)
    int Register(hks::lua_State* L);

    // Cleanup
    void Shutdown();
}