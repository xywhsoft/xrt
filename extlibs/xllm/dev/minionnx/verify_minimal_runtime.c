#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#include "onnxruntime/core/session/onnxruntime_c_api.h"

typedef const OrtApiBase*(__cdecl *OrtGetApiBaseFn)(void);

static void print_last_error(const char* label) {
    DWORD error = GetLastError();
    fprintf(stderr, "%s failed with Win32 error %lu\n", label, (unsigned long)error);
}

static wchar_t* utf8_to_utf16(const char* input) {
    int required = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0);
    if (required <= 0) {
        return NULL;
    }

    wchar_t* buffer = (wchar_t*)calloc((size_t)required, sizeof(wchar_t));
    if (!buffer) {
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, input, -1, buffer, required) <= 0) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <onnxruntime.dll> <model.ort>\n", argv[0]);
        return 2;
    }

    const char* dll_path = argv[1];
    const char* model_path_utf8 = argv[2];

    HMODULE ort_dll = LoadLibraryA(dll_path);
    if (!ort_dll) {
        print_last_error("LoadLibraryA");
        return 1;
    }

    OrtGetApiBaseFn get_api_base = (OrtGetApiBaseFn)GetProcAddress(ort_dll, "OrtGetApiBase");
    if (!get_api_base) {
        print_last_error("GetProcAddress(OrtGetApiBase)");
        FreeLibrary(ort_dll);
        return 1;
    }

    const OrtApiBase* api_base = get_api_base();
    if (!api_base) {
        fprintf(stderr, "OrtGetApiBase returned NULL\n");
        FreeLibrary(ort_dll);
        return 1;
    }

    const OrtApi* api = api_base->GetApi(ORT_API_VERSION);
    if (!api) {
        fprintf(stderr, "GetApi returned NULL for ORT_API_VERSION=%d\n", ORT_API_VERSION);
        FreeLibrary(ort_dll);
        return 1;
    }

    wchar_t* model_path = utf8_to_utf16(model_path_utf8);
    if (!model_path) {
        fprintf(stderr, "Failed to convert model path to UTF-16\n");
        FreeLibrary(ort_dll);
        return 1;
    }

    OrtEnv* env = NULL;
    OrtSessionOptions* session_options = NULL;
    OrtSession* session = NULL;
    OrtStatus* status = NULL;
    int exit_code = 1;

    status = api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "minionnx_verify", &env);
    if (status) {
        fprintf(stderr, "CreateEnv failed: %s\n", api->GetErrorMessage(status));
        api->ReleaseStatus(status);
        goto cleanup;
    }

    status = api->CreateSessionOptions(&session_options);
    if (status) {
        fprintf(stderr, "CreateSessionOptions failed: %s\n", api->GetErrorMessage(status));
        api->ReleaseStatus(status);
        goto cleanup;
    }

    status = api->CreateSession(env, model_path, session_options, &session);
    if (status) {
        fprintf(stderr, "CreateSession failed: %s\n", api->GetErrorMessage(status));
        api->ReleaseStatus(status);
        goto cleanup;
    }

    printf("Minimal runtime loaded successfully.\n");
    printf("DLL   : %s\n", dll_path);
    printf("Model : %s\n", model_path_utf8);
    exit_code = 0;

cleanup:
    if (session) {
        api->ReleaseSession(session);
    }
    if (session_options) {
        api->ReleaseSessionOptions(session_options);
    }
    if (env) {
        api->ReleaseEnv(env);
    }
    free(model_path);
    FreeLibrary(ort_dll);
    return exit_code;
}
