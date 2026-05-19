#include <SDL3/SDL.h>
#include <stdio.h>
#include <signal.h>
#include "mcp.h"

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#endif

// Forward declaration from wl_main.c
extern void wolf_main(void);

// Borland-style command line globals
int _argc;
char **_argv;

// Borland register variables (stubs)
unsigned short _AX, _BX, _CX, _DX, _SI, _DI, _BP, _SP, _DS, _ES, _SS;

void SDL3_Delay(unsigned int ms) {
    SDL_Delay(ms);
}

#ifdef _WIN32
static HANDLE g_symProcess = NULL;

static LONG WINAPI ExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;

    // Ignore non-fatal "exceptions" raised by the OS / debug helpers
    if (code == 0x406D1388) { // MS_VC_EXCEPTION (thread naming)
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code == 0x40010006) { // DBG_PRINTEXCEPTION_C (OutputDebugString)
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code == 0x4001000A) { // DBG_PRINTEXCEPTION_WIDE_C
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    FILE *f = fopen("crash_log.txt", "w");
    HANDLE process = g_symProcess ? g_symProcess : GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    if (f) {
        fprintf(f, "Exception caught!\n");
        fprintf(f, "Exception code: 0x%08X\n", (unsigned)ExceptionInfo->ExceptionRecord->ExceptionCode);
        fprintf(f, "Exception address: %p\n", ExceptionInfo->ExceptionRecord->ExceptionAddress);
    }
    fprintf(stderr, "Exception caught!\n");
    fprintf(stderr, "Exception code: 0x%08X\n", (unsigned)ExceptionInfo->ExceptionRecord->ExceptionCode);
    fprintf(stderr, "Exception address: %p\n", ExceptionInfo->ExceptionRecord->ExceptionAddress);

    STACKFRAME64 stackFrame;
    DWORD machineType;
    CONTEXT context = *ExceptionInfo->ContextRecord;

    memset(&stackFrame, 0, sizeof(STACKFRAME64));

#ifdef _M_X64
    machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#else
    machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#endif

    char symbolBuffer[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)symbolBuffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 511;

    DWORD64 displacement;
    int frameNum = 0;

    while (StackWalk64(machineType, process, thread, &stackFrame, &context,
                       NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
        if (stackFrame.AddrPC.Offset == 0)
            break;

        DWORD symErr = GetLastError();
        if (SymFromAddr(process, stackFrame.AddrPC.Offset, &displacement, symbol)) {
            if (f) fprintf(f, "%d: %s + 0x%llX (0x%llX)\n", frameNum, symbol->Name,
                    displacement, stackFrame.AddrPC.Offset);
            fprintf(stderr, "%d: %s + 0x%llX (0x%llX)\n", frameNum, symbol->Name,
                    displacement, stackFrame.AddrPC.Offset);
        } else {
            HMODULE hMod = NULL;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                               (LPCSTR)stackFrame.AddrPC.Offset, &hMod);
            uintptr_t rel = (uintptr_t)stackFrame.AddrPC.Offset - (uintptr_t)hMod;
            if (f) fprintf(f, "%d: 0x%llX (rel=0x%llX mod=%p err=%lu)\n", frameNum,
                    stackFrame.AddrPC.Offset, (unsigned long long)rel, (void*)hMod, (unsigned long)symErr);
            fprintf(stderr, "%d: 0x%llX (rel=0x%llX mod=%p err=%lu)\n", frameNum,
                    stackFrame.AddrPC.Offset, (unsigned long long)rel, (void*)hMod, (unsigned long)symErr);
        }
        frameNum++;
        if (frameNum > 64) break;
    }

    if (f) fclose(f);
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    _argc = argc;
    _argv = argv;

#ifdef _WIN32
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    g_symProcess = GetCurrentProcess();
    if (!SymInitialize(g_symProcess, NULL, TRUE)) {
        DWORD err = GetLastError();
        fprintf(stderr, "SymInitialize failed: 0x%08X (continuing without symbols)\n", (unsigned)err);
        g_symProcess = NULL;
    }

    SetUnhandledExceptionFilter(ExceptionHandler);
    AddVectoredExceptionHandler(1, ExceptionHandler);
#endif

    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    MCP_Init();
    wolf_main();

    SDL_Quit();
    return 0;
}
