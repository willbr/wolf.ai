// mcp.c - MCP server thread built into the game
// Communicates over stdio via JSON-RPC 2.0

#include "mcp.h"
#include "wl_def.h"
#include "id_in.h"
#include "id_vl.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// External game state (defined in various game modules)
extern boolean ingame;
extern boolean startgame;
extern boolean loadedgame;
extern gametype gamestate;

// Screenshot buffer (defined in id_vl.c)
extern Uint32 linear_buffer[320 * 200];

static SDL_Thread *mcp_thread = NULL;
static volatile int mcp_running = 0;

//
// Simple TGA writer (uncompressed RGBA, top-left origin)
//
static int WriteTGAScreenshot(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    unsigned char header[18];
    memset(header, 0, sizeof(header));
    header[2]  = 2;          // uncompressed true-color
    header[12] = 320 & 0xFF; // width low
    header[13] = 320 >> 8;   // width high
    header[14] = 200 & 0xFF; // height low
    header[15] = 200 >> 8;   // height high
    header[16] = 32;         // bits per pixel
    header[17] = 0x28;       // top-left origin, 8 bits alpha
    fwrite(header, 1, 18, f);

    // TGA stores BGRA, linear_buffer is RGBA
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
            Uint32 c = linear_buffer[y * 320 + x];
            unsigned char bgra[4];
            bgra[0] = (c >> 16) & 0xFF; // B
            bgra[1] = (c >> 8)  & 0xFF; // G
            bgra[2] = (c >> 0)  & 0xFF; // R
            bgra[3] = (c >> 24) & 0xFF; // A
            fwrite(bgra, 1, 4, f);
        }
    }
    fclose(f);
    return 0;
}

//
// Send JSON-RPC response / error
//
static void mcp_respond(int id, const char *result)
{
    printf("{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}\n", id, result);
    fflush(stdout);
}

static void mcp_error(int id, int code, const char *msg)
{
    printf("{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%d,\"message\":\"%s\"}}\n", id, code, msg);
    fflush(stdout);
}

//
// Tiny JSON value extractor
// Returns pointer to value start (after the key), or NULL
//
static const char *json_find(const char *json, const char *key)
{
    const char *p = strstr(json, key);
    if (!p) return NULL;
    p += strlen(key);
    while (*p == ' ' || *p == '\t' || *p == ':' || *p == '"') p++;
    return p;
}

//
// Command handlers
//
static void mcp_send_key(int id, const char *params)
{
    const char *p = json_find(params, "scancode");
    int sc = p ? atoi(p) : 0;

    if (sc > 0 && sc < NumCodes) {
        Keyboard[sc] = true;
        LastScan = (ScanCode)sc;
        SDL_Delay(200);
        IN_ClearKeysDown();
    }
    mcp_respond(id, "{\"status\":\"sent\"}");
}

// Extern menu state from wl_menu.c
extern const char *mcp_current_menu_name;
extern int mcp_selected_item;
extern int mcp_menu_option_count;
extern char mcp_menu_options[12][36];

static void mcp_get_state(int id)
{
    char buf[2048];
    char opts[1024] = "[";
    for (int i = 0; i < mcp_menu_option_count && i < 12; i++) {
        if (i > 0) strcat(opts, ",");
        strcat(opts, "\"");
        // Escape JSON special chars in option strings
        char escaped[160];
        int k = 0;
        for (int j = 0; mcp_menu_options[i][j] && k < 150; j++) {
            unsigned char c = mcp_menu_options[i][j];
            if (c == '"' || c == '\\') {
                escaped[k++] = '\\';
                escaped[k++] = c;
            } else if (c == '\n') {
                escaped[k++] = '\\';
                escaped[k++] = 'n';
            } else if (c == '\r') {
                escaped[k++] = '\\';
                escaped[k++] = 'r';
            } else if (c == '\t') {
                escaped[k++] = '\\';
                escaped[k++] = 't';
            } else if (c < 0x20) {
                // skip other control chars
            } else {
                escaped[k++] = c;
            }
        }
        escaped[k] = '\0';
        strcat(opts, escaped);
        strcat(opts, "\"");
    }
    strcat(opts, "]");

    const char *menu = mcp_current_menu_name ? mcp_current_menu_name : "null";

    snprintf(buf, sizeof(buf),
        "{\"ingame\":%s,\"startgame\":%s,\"loadedgame\":%s,\"mapon\":%d,\"episode\":%d,\"difficulty\":%d,\"menu\":\"%s\",\"selected\":%d,\"options\":%s}",
        ingame ? "true" : "false",
        startgame ? "true" : "false",
        loadedgame ? "true" : "false",
        gamestate.mapon,
        gamestate.episode,
        gamestate.difficulty,
        menu,
        mcp_selected_item,
        opts);
    mcp_respond(id, buf);
}

static void mcp_take_screenshot(int id)
{
    const char *tmp = "temp";
    char path[512];
    snprintf(path, sizeof(path), "%s/wolf3d_screenshot.tga", tmp);

    if (WriteTGAScreenshot(path) != 0) {
        mcp_error(id, -32000, "Failed to write screenshot");
        return;
    }

    // Windows paths have backslashes; use forward slashes for valid JSON
    char json_path[512];
    strncpy(json_path, path, sizeof(json_path) - 1);
    json_path[sizeof(json_path) - 1] = '\0';
    for (int i = 0; json_path[i]; i++) {
        if (json_path[i] == '\\') json_path[i] = '/';
    }

    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"path\":\"%s\",\"width\":320,\"height\":200,\"format\":\"tga\"}",
        json_path);
    mcp_respond(id, buf);
}

static void mcp_navigate(int id)
{
    // Step 1: break out of title-screen / attract-mode
    Keyboard[sc_Return] = true; LastScan = sc_Return;
    SDL_Delay(200);
    IN_ClearKeysDown();
    SDL_Delay(4000);

    // Step 2: Main Menu -> New Game
    Keyboard[sc_Return] = true; LastScan = sc_Return;
    SDL_Delay(200);
    IN_ClearKeysDown();
    SDL_Delay(4000);

    // Step 3: Episode select -> Episode 1
    Keyboard[sc_Return] = true; LastScan = sc_Return;
    SDL_Delay(200);
    IN_ClearKeysDown();
    SDL_Delay(4000);

    // Step 4: Difficulty select -> first difficulty
    Keyboard[sc_Return] = true; LastScan = sc_Return;
    SDL_Delay(200);
    IN_ClearKeysDown();
    SDL_Delay(5000);

    mcp_respond(id, "{\"status\":\"in_gameplay\"}");
}

static void mcp_alive(int id)
{
    mcp_respond(id, "{\"alive\":true}");
}

static void mcp_initialize(int id)
{
    mcp_respond(id, "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"serverInfo\":{\"name\":\"wolf3d-mcp\",\"version\":\"2.0.0\"}}");
}

static void mcp_tools_list(int id)
{
    mcp_respond(id,
        "{\"tools\":["
        "{\"name\":\"send_key\",\"description\":\"Send a keypress\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"scancode\":{\"type\":\"integer\"}},\"required\":[\"scancode\"]}}"
        ",{\"name\":\"get_state\",\"description\":\"Get game state\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
        ",{\"name\":\"take_screenshot\",\"description\":\"Take a screenshot\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
        ",{\"name\":\"navigate_to_gameplay\",\"description\":\"Navigate menus into gameplay\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
        ",{\"name\":\"is_process_alive\",\"description\":\"Check if process is alive\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
        "]}");
}

//
// Dispatch a JSON-RPC request line
//
static void mcp_dispatch(const char *line)
{
    int id = -1;
    const char *p = json_find(line, "\"id\"");
    if (p) id = atoi(p);

    const char *method_p = json_find(line, "\"method\"");
    if (!method_p) {
        mcp_error(id, -32600, "Missing method");
        return;
    }

    char method[64] = {0};
    int i = 0;
    while (*method_p && *method_p != '"' && i < 63) {
        method[i++] = *method_p++;
    }

    const char *params = strstr(line, "\"params\"");

    if (strcmp(method, "send_key") == 0) {
        mcp_send_key(id, params ? params : "");
    } else if (strcmp(method, "get_state") == 0) {
        mcp_get_state(id);
    } else if (strcmp(method, "take_screenshot") == 0) {
        mcp_take_screenshot(id);
    } else if (strcmp(method, "navigate_to_gameplay") == 0) {
        mcp_navigate(id);
    } else if (strcmp(method, "is_process_alive") == 0) {
        mcp_alive(id);
    } else if (strcmp(method, "initialize") == 0) {
        mcp_initialize(id);
    } else if (strcmp(method, "tools/list") == 0) {
        mcp_tools_list(id);
    } else if (strcmp(method, "tools/call") == 0) {
        // Extract tool name from nested params
        const char *name_p = json_find(params, "\"name\"");
        char tool[64] = {0};
        if (name_p) {
            int j = 0;
            while (*name_p && *name_p != '"' && j < 63) {
                tool[j++] = *name_p++;
            }
        }
        const char *tool_params = strstr(params ? params : "", "\"arguments\"");
        if (strcmp(tool, "send_key") == 0) {
            mcp_send_key(id, tool_params ? tool_params : "");
        } else if (strcmp(tool, "get_state") == 0) {
            mcp_get_state(id);
        } else if (strcmp(tool, "take_screenshot") == 0) {
            mcp_take_screenshot(id);
        } else if (strcmp(tool, "navigate_to_gameplay") == 0) {
            mcp_navigate(id);
        } else if (strcmp(tool, "is_process_alive") == 0) {
            mcp_alive(id);
        } else {
            mcp_error(id, -32601, "Unknown tool");
        }
    } else if (strcmp(method, "notifications/initialized") == 0) {
        // no response
    } else {
        mcp_error(id, -32601, "Unknown method");
    }
}

//
// MCP thread entry point
//
static int MCP_Thread(void *data)
{
    (void)data;
    char line[4096];

    while (mcp_running) {
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        // strip newline / carriage return
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        if (len > 0 && line[len-1] == '\r') line[--len] = '\0';
        if (len == 0) continue;

        mcp_dispatch(line);
    }
    return 0;
}

void MCP_Init(void)
{
    mcp_running = 1;
    mcp_thread = SDL_CreateThread(MCP_Thread, "MCP", NULL);
    if (!mcp_thread) {
        fprintf(stderr, "MCP_Init: failed to create thread\n");
    }
}

void MCP_Shutdown(void)
{
    mcp_running = 0;
    if (mcp_thread) {
        SDL_WaitThread(mcp_thread, NULL);
        mcp_thread = NULL;
    }
}
