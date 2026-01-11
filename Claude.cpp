#include "Claude.h"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdlib>

// Link with winhttp.lib
#pragma comment(lib, "winhttp.lib")

// Include JSON
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace Claude {
    // Configuration
    static std::string g_apiKey = "";
    static std::string g_model = "claude-sonnet-4-5-20250929";
    static int g_maxTokens = 4096;

    // Async state
    static std::queue<std::string> g_responseQueue;
    static std::mutex g_queueMutex;
    static std::atomic<bool> g_asyncPending(false);
    static std::thread g_asyncThread;

    // Cooldown - one query per turn
    static int g_lastQueryTurn = -1;

    // Helper to get API key from environment or stored variable
    static std::string GetAPIKey() {
        if (!g_apiKey.empty()) {
            return g_apiKey;
        }
        char *envKey = nullptr;
        size_t len = 0;
        if (_dupenv_s(&envKey, &len, "ANTHROPIC_API_KEY") == 0 && envKey != nullptr) {
            std::string key(envKey);
            free(envKey);
            return key;
        }
        return "";
    }

    // Helper to escape JSON strings
    static std::string EscapeJson(const std::string& s) {
        std::ostringstream o;
        for (char c : s) {
            switch (c) {
                case '"': o << "\\\""; break;
                case '\\': o << "\\\\"; break;
                case '\b': o << "\\b"; break;
                case '\f': o << "\\f"; break;
                case '\n': o << "\\n"; break;
                case '\r': o << "\\r"; break;
                case '\t': o << "\\t"; break;
                default:
                    if ('\x00' <= c && c <= '\x1f') {
                        o << "\\u"
                          << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                    } else {
                        o << c;
                    }
            }
        }
        return o.str();
    }

    // Helper to convert UTF-8 to wide string
    static std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
        std::wstring wide_str(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wide_str[0], size_needed);
        return wide_str;
    }

    void SetAPIKey(const std::string& key) {
        g_apiKey = key;
        std::cout << "Claude API Key set.\n";
    }

    void SetModel(const std::string& model) {
        g_model = model;
        std::cout << "Claude model set to: " << g_model << "\n";
    }

    void SetMaxTokens(int maxTokens) {
        g_maxTokens = maxTokens;
        std::cout << "Claude max tokens set to: " << g_maxTokens << "\n";
    }

    std::string Query(const std::string& prompt, const std::string& systemPrompt) {
        std::string apiKey = GetAPIKey();
        if (apiKey.empty()) {
            return "Error: Claude API key not set.";
        }

        std::cout << "[Claude] Sending query...\n";

        // Build JSON request body
        json requestBody;
        requestBody["model"] = g_model;
        requestBody["max_tokens"] = g_maxTokens;

        if (!systemPrompt.empty()) {
            requestBody["system"] = systemPrompt;
        }

        requestBody["messages"] = json::array({
            { {"role", "user"}, {"content", prompt} }
        });

        std::string jsonBody = requestBody.dump();

        // Initialize WinHTTP
        HINTERNET hSession = WinHttpOpen(
            L"Civ6-Claude/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );

        if (!hSession) {
            return "Error: Failed to open WinHTTP session.";
        }

        // Connect to API
        HINTERNET hConnect = WinHttpConnect(
            hSession,
            L"api.anthropic.com",
            INTERNET_DEFAULT_HTTPS_PORT,
            0
        );

        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return "Error: Failed to connect to Claude API.";
        }

        // Create request
        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"POST",
            L"/v1/messages",
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        );
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "Error: Failed to open HTTP request.";
        }

        // Build headers
        std::wstring headers = L"Content-Type: application/json\r\n";
        headers += L"x-api-key: " + Utf8ToWide(apiKey) + L"\r\n";
        headers += L"anthropic-version: 2023-06-01\r\n";

        BOOL bResults = WinHttpSendRequest(
            hRequest,
            headers.c_str(),
            (DWORD)-1L,
            (LPVOID)jsonBody.c_str(),
            (DWORD)jsonBody.length(),
            (DWORD)jsonBody.length(),
            0
        );

        if (!bResults) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "Error: Failed to send HTTP request. Error code: " + std::to_string(GetLastError());
        }

        // Receive response
        bResults = WinHttpReceiveResponse(hRequest, NULL);
        
        if (!bResults) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "Error: Failed to receive HTTP response.";
        }

        // Check status code
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusCodeSize,
            WINHTTP_NO_HEADER_INDEX
        );

        // Read response data
        std::string responseStr;
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;

        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                responseStr = "Error: Failed to query data availability.";
                break;
            }

            if (dwSize == 0) break;

            std::vector<char> buffer(dwSize+1, 0);
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                responseStr.append(buffer.data(), dwDownloaded);
            } else {
                responseStr = "Error: Failed to read response data.";
                break;
            }
        } while (dwSize > 0);

        // Cleanup
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // Parse response
        if (statusCode != 200) {
            return "Error: Claude API returned status code " + std::to_string(statusCode) + ". Response: " + responseStr;
        }

        try {
            json response = json::parse(responseStr);

            if (response.contains("content") && response["content"].is_array() && !response["content"].empty()) {
                for (const auto& block : response["content"]) {
                    if (block.contains("type") && block["type"] == "text" && block.contains("text")) {
                        std::cout << "[Claude] Response received.\n";
                        return block["text"].get<std::string>();
                    }
                }
            }

            if (response.contains("error")) {
                return "Error from Claude API: " + response["error"].get<std::string>();
            }

            return "Error: Unexpected response format from Claude API: " + responseStr;
        }
        catch (const json::exception& e) {
            return std::string("Error: Failed to parse JSON response: ") + e.what();
        }
    }

    void QueryAsync(const std::string& prompt, const std::string& systemPrompt) {
        // Wait for previous async operations to complete
        if (g_asyncThread.joinable()) {
            g_asyncThread.join();
        }

        g_asyncPending = true;

        // Launch query in the background
        g_asyncThread = std::thread([prompt, systemPrompt]() {
            std::string response = Query(prompt, systemPrompt);
            {
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_responseQueue.push(response);
            }
            g_asyncPending = false;
        });
    }

    std::string QueryForTurn(int turnNumber, const std::string& prompt, const std::string& systemPrompt) {
        if (turnNumber == g_lastQueryTurn) {
            return "Error: Only one Claude query allowed per turn.";
        }
        g_lastQueryTurn = turnNumber;
        return Query(prompt, systemPrompt);
    }

    void QueryForTurnAsync(int turnNumber, const std::string& prompt, const std::string& systemPrompt) {
        if (turnNumber == g_lastQueryTurn) {
            std::lock_guard<std::mutex> lock(g_queueMutex);
            g_responseQueue.push("Error: Only one Claude query allowed per turn.");
            return;
        }
        g_lastQueryTurn = turnNumber;
        QueryAsync(prompt, systemPrompt);
    }

    bool HasResponse() {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        return !g_responseQueue.empty();
    }

    std::string GetResponse() {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        if (g_responseQueue.empty()) {
            return "";
        }
        std::string response = g_responseQueue.front();
        g_responseQueue.pop();
        return response;
    }

    void Shutdown() {
        if (g_asyncThread.joinable()) {
            g_asyncThread.join();
        }
    }

    /*** Lua Bindings  ***/

    // Claude.Query(prompt [, systemPrompt]) -> string
    int lua_Query(hks::lua_State* L) {
        const char* prompt = hks::checklstring(L, 1, nullptr);
        const char* systemPrompt = "";

        if (hks::gettop(L) >= 2) {
            systemPrompt = hks::checklstring(L, 2, nullptr);
        }

        std::string result = Query(prompt, systemPrompt);

        // Push onto Lua stack
        hks::pushfstring(L, "%s", result.c_str());

        return 1; // Return one value
    }

    // Claude.QueryAsync(prompt [, systemPrompt]) -> nil
    int lua_QueryAsync(hks::lua_State* L) {
        const char* prompt = hks::checklstring(L, 1, nullptr);
        const char* systemPrompt = "";

        if (hks::gettop(L) >= 2) {
            systemPrompt = hks::checklstring(L, 2, nullptr);
        }

        QueryAsync(prompt, systemPrompt);

        return 0; // No return values
    }

    // Claude.QueryForTurn(turnNumber, prompt [, systemPrompt]) -> string
    int lua_QueryForTurn(hks::lua_State* L) {
        int turnNumber = hks::checkinteger(L, 1);
        const char* prompt = hks::checklstring(L, 2, nullptr);
        const char* systemPrompt = "";

        if (hks::gettop(L) >= 3) {
            systemPrompt = hks::checklstring(L, 3, nullptr);
        }

        std::string result = QueryForTurn(turnNumber, prompt, systemPrompt);
        hks::pushfstring(L, "%s", result.c_str());

        return 1;
    }

    // Claude.QueryForTurnAsync(turnNumber, prompt [, systemPrompt]) -> nil
    int lua_QueryForTurnAsync(hks::lua_State* L) {
        int turnNumber = hks::checkinteger(L, 1);
        const char* prompt = hks::checklstring(L, 2, nullptr);
        const char* systemPrompt = "";

        if (hks::gettop(L) >= 3) {
            systemPrompt = hks::checklstring(L, 3, nullptr);
        }

        QueryForTurnAsync(turnNumber, prompt, systemPrompt);

        return 0;
    }

    // Claude.HasResponse() -> bool
    int lua_HasResponse(hks::lua_State* L) {
        hks::pushboolean(L, HasResponse());
        return 1; // Return one value
    }

    // Claude.GetResponse() -> string
    int lua_GetResponse(hks::lua_State* L) {
        std::string response = GetResponse();
        hks::pushfstring(L, "%s", response.c_str());
        return 1; // Return one value
    }

    // Claude.SetAPIKey(apiKey) -> nil
    int lua_SetAPIKey(hks::lua_State* L) {
        const char* apiKey = hks::checklstring(L, 1, nullptr);
        SetAPIKey(apiKey);
        return 0; // No return values
    }

    // Claude.SetModel(model) -> nil
    int lua_SetModel(hks::lua_State* L) {
        const char* model = hks::checklstring(L, 1, nullptr);
        SetModel(model);
        return 0; // No return values
    }

    // Claude.SetMaxTokens(maxTokens) -> nil
    int lua_SetMaxTokens(hks::lua_State* L) {
        int maxTokens = hks::checkinteger(L, 1);
        SetMaxTokens(maxTokens);
        return 0; // No return values
    }

    // Register all Claude functions as global "Claude" table
    int Register(hks::lua_State* L) {
        std::cout << "[Claude] Registering Lua bindings...\n";

        // Create the Claude table
        hks::createtable(L, 0, 9); // 9 functions

        // Add functions
        hks::pushnamedcclosure(L, lua_Query, 0, "lua_Query", 0);
        hks::setfield(L, -2, "Query");

        hks::pushnamedcclosure(L, lua_QueryAsync, 0, "lua_QueryAsync", 0);
        hks::setfield(L, -2, "QueryAsync");

        hks::pushnamedcclosure(L, lua_QueryForTurn, 0, "lua_QueryForTurn", 0);
        hks::setfield(L, -2, "QueryForTurn");

        hks::pushnamedcclosure(L, lua_QueryForTurnAsync, 0, "lua_QueryForTurnAsync", 0);
        hks::setfield(L, -2, "QueryForTurnAsync");

        hks::pushnamedcclosure(L, lua_HasResponse, 0, "lua_HasResponse", 0);
        hks::setfield(L, -2, "HasResponse");

        hks::pushnamedcclosure(L, lua_GetResponse, 0, "lua_GetResponse", 0);
        hks::setfield(L, -2, "GetResponse");

        hks::pushnamedcclosure(L, lua_SetAPIKey, 0, "lua_SetAPIKey", 0);
        hks::setfield(L, -2, "SetAPIKey");

        hks::pushnamedcclosure(L, lua_SetModel, 0, "lua_SetModel", 0);
        hks::setfield(L, -2, "SetModel");

        hks::pushnamedcclosure(L, lua_SetMaxTokens, 0, "lua_SetMaxTokens", 0);
        hks::setfield(L, -2, "SetMaxTokens");

        // Set the table as global "Claude"
        hks::setfield(L, hks::LUA_GLOBAL, "Claude");

        std::cout << "[Claude] Lua bindings registered successfully.\n";
        return 0;
    }
}