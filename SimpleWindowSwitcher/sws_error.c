#include "sws_error.h"

void sws_error_PrintStackTrace(void)
{
#if _WIN64
    DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#else
    DWORD      machine = IMAGE_FILE_MACHINE_I386;
#endif
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
    HANDLE thread  = GetCurrentThread();

    if (SymInitializeW(process, NULL, TRUE) == FALSE)
        return;

    SymSetOptions(SYMOPT_LOAD_LINES);

    CONTEXT context;
    ZeroMemory(&context, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);

#if _WIN64
    STACKFRAME frame;
    ZeroMemory(&frame, sizeof(STACKFRAME));
    frame.AddrPC.Offset    = context.Rip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode   = AddrModeFlat;
#else
    STACKFRAME frame;
    ZeroMemory(&frame, sizeof(STACKFRAME));
    frame.AddrPC.Offset    = context.Eip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode   = AddrModeFlat;
#endif

    UINT i = 0;
    while (StackWalk(machine, process, thread, &frame, &context, NULL,
                     SymFunctionTableAccess, SymGetModuleBase, NULL))
    {
        printf("[%3d] = [0x%p] :: ", i, (void *)frame.AddrPC.Offset);

#if _WIN64
        DWORD64 moduleBase;
#else
        DWORD moduleBase;
#endif
        moduleBase = SymGetModuleBase(process, frame.AddrPC.Offset);

        char moduelBuff[MAX_PATH];
        if (moduleBase && GetModuleFileNameA((HINSTANCE)moduleBase, moduelBuff, MAX_PATH)) {
        }

#if _WIN64
        DWORD64 offset = 0;
#else
        DWORD offset     = 0;
#endif
        char symbolBuffer[sizeof(IMAGEHLP_SYMBOL) + 255];
        PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL)symbolBuffer;
        symbol->SizeOfStruct    = (sizeof(IMAGEHLP_SYMBOL)) + 255;
        symbol->MaxNameLength   = 254;

        if (SymGetSymFromAddr(process, frame.AddrPC.Offset, &offset, symbol)) {
            printf("%s:", symbol->Name);
        }

        IMAGEHLP_LINE line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE);

        DWORD offset_ln = 0;
        if (SymGetLineFromAddr(process, frame.AddrPC.Offset, &offset_ln, &line)) {
            wprintf(L"%d in file \"%s\"", line.LineNumber, line.FileName);
        } else {
        }

        putchar('\n');
        ++i;
    }

    SymCleanup(process);
    CloseHandle(process);
}

char *
sws_error_NumToDescription(sws_error_t errnum, BOOL *bType)
{
    if (bType)
        *bType = FALSE;
    char *ret = NULL;
    switch (errnum) {
    case SWS_ERROR_SUCCESS:
        ret = SWS_ERROR_SUCCESS_TEXT;
        break;
    case SWS_ERROR_ERROR:
        ret = SWS_ERROR_ERROR_TEXT;
        break;
    case SWS_ERROR_GENERIC_ERROR:
        ret = SWS_ERROR_GENERIC_ERROR_TEXT;
        break;
    case SWS_ERROR_NO_MEMORY:
        ret = SWS_ERROR_NO_MEMORY_TEXT;
        break;
    case SWS_ERROR_NOT_INITIALIZED:
        ret = SWS_ERROR_NOT_INITIALIZED_TEXT;
        break;
    case SWS_ERROR_LOADLIBRARY_FAILED:
        ret = SWS_ERROR_LOADLIBRARY_FAILED_TEXT;
        break;
    case SWS_ERROR_FUNCTION_NOT_FOUND:
        ret = SWS_ERROR_FUNCTION_NOT_FOUND_TEXT;
        break;
    case SWS_ERROR_UNABLE_TO_SET_DPI_AWARENESS_CONTEXT:
        ret = SWS_ERROR_UNABLE_TO_SET_DPI_AWARENESS_CONTEXT_TEXT;
        break;
    case SWS_ERROR_INVALID_PARAMETER:
        ret = SWS_ERROR_INVALID_PARAMETER_TEXT;
        break;
    case SWS_ERROR_SHELL_NOT_FOUND:
        ret = SWS_ERROR_SHELL_NOT_FOUND;
        break;
    case SWS_ERROR_NOERROR_JUST_PRINT_STACKTRACE:
        ret = SWS_ERROR_NOERROR_JUST_PRINT_STACKTRACE_TEXT;
        break;
    default:
        if (bType)
            *bType = TRUE;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
                       NULL, errnum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPSTR)&ret, 0, NULL);
        break;
    }
    return ret;
}

sws_error_t
sws_error_Report(sws_error_t errnum, void *data)
{
    if (errnum == SWS_ERROR_SUCCESS)
        return errnum;
    char *errdesc;

    _lock_file(stdout);
    printf("====================================\nAn error occurred in the application.\nError number: 0x%lX\n", errnum);

    BOOL bShouldFree = FALSE;
    if ((errdesc = sws_error_NumToDescription(errnum, &bShouldFree))) {
        printf("Description: %s\n", errdesc);
        if (bShouldFree)
            LocalFree(errdesc);
    } else {
        puts(".");
    }
    if (data)
        printf("Additional data: 0x%p\n", data);
    puts("Here is the stack trace:");
    sws_error_PrintStackTrace();

    puts("====================================");
    _unlock_file(stdout);
    return errnum;
}