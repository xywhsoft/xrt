#if defined(_WIN32) || defined(_WIN64)
#include "../test.h"
#include <windows.h>
#include <wchar.h>

int main(void)
{
	wchar_t Temp[MAX_PATH], Folder[MAX_PATH], Wrapper[MAX_PATH * 2u];
	wchar_t* Previous;
	wchar_t* Search;
	DWORD Capacity, Written;
	HANDLE File;
	xprocessresult Result = {0};
	cstr Args[] = {"/d", "/c", "echo resolved"};
	bool Ok;
	static const char Script[] = "#!/bin/sh\nexit 99\n";
	testRequire(GetTempPathW(MAX_PATH, Temp) != 0u &&
		GetTempFileNameW(Temp, L"xrt", 0u, Folder) != 0u,
		"program resolution temporary path failed");
	testRequire(DeleteFileW(Folder) && CreateDirectoryW(Folder, NULL),
		"program resolution temporary directory failed");
	testRequire(swprintf(Wrapper, MAX_PATH * 2u, L"%ls\\cmd", Folder) > 0,
		"program resolution wrapper path failed");
	File = CreateFileW(Wrapper, GENERIC_WRITE, 0u, NULL, CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL, NULL);
	testRequire(File != INVALID_HANDLE_VALUE &&
		WriteFile(File, Script, (DWORD)(sizeof(Script) - 1u), &Written, NULL) &&
		Written == sizeof(Script) - 1u && CloseHandle(File),
		"program resolution shadow wrapper creation failed");
	Capacity = GetEnvironmentVariableW(L"PATH", NULL, 0u);
	testRequire(Capacity != 0u, "program resolution PATH query failed");
	Previous = malloc((size_t)Capacity * sizeof(*Previous));
	Search = malloc(((size_t)Capacity + MAX_PATH + 1u) * sizeof(*Search));
	testRequire(Previous != NULL && Search != NULL &&
		GetEnvironmentVariableW(L"PATH", Previous, Capacity) != 0u,
		"program resolution PATH snapshot failed");
	testRequire(swprintf(Search, (size_t)Capacity + MAX_PATH + 1u,
		L"%ls;%ls", Folder, Previous) > 0 &&
		SetEnvironmentVariableW(L"PATH", Search),
		"program resolution shadow PATH setup failed");
	/* The extensionless script must not shadow the native cmd.exe. */
	Ok = xrtProcessCapture("cmd", Args, 3u, &Result);
	testRequire(SetEnvironmentVariableW(L"PATH", Previous),
		"program resolution PATH restoration failed");
	free(Search);
	free(Previous);
	testRequire(DeleteFileW(Wrapper) && RemoveDirectoryW(Folder),
		"program resolution temporary cleanup failed");
	testRequire(Ok && xrtProcessResultSuccess(&Result) &&
		Result.StdoutSize == 10u && Result.StderrSize == 0u &&
		memcmp(Result.Stdout, "resolved\r\n", 10u) == 0,
		"extensionless wrapper shadowed the native Windows executable");
	xrtProcessResultUnit(&Result);
	xrtClearError();
	puts("[PASS] native Windows executable takes precedence over a shell wrapper");
	return 0;
}
#else
#include <stdio.h>
int main(void)
{
	puts("[SKIP] Windows executable search semantics");
	return 0;
}
#endif
