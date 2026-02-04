

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#define MAX_PATH 4096


typedef struct {
	char path[MAX_PATH];
	char* name;
} FileEntry;


FileEntry g_Files[] = {
	{ "xrt.h", "xrt.h" },
	{ "lib/suplib.h", "lib/suplib.h" },
	{ "lib/base.h", "lib/base.h" },
	{ "lib/charset.h", "lib/charset.h" },
	{ "lib/os.h", "lib/os.h" },
	{ "lib/math.h", "lib/math.h" },
	{ "lib/string.h", "lib/string.h" },
	{ "lib/path.h", "lib/path.h" },
	{ "lib/time.h", "lib/time.h" },
	{ "lib/file.h", "lib/file.h" },
	{ "lib/thread.h", "lib/thread.h" },
	{ "lib/hash.h", "lib/hash.h" },
	{ "lib/network.h", "lib/network.h" },
	{ "lib/xid.h", "lib/xid.h" },
	{ "lib/buffer.h", "lib/buffer.h" },
	{ "lib/array_point.h", "lib/array_point.h" },
	{ "lib/array.h", "lib/array.h" },
	{ "lib/bsmm.h", "lib/bsmm.h" },
	{ "lib/memunit.h", "lib/memunit.h" },
	{ "lib/mempool_fs.h", "lib/mempool_fs.h" },
	{ "lib/stack.h", "lib/stack.h" },
	{ "lib/stack_dyn.h", "lib/stack_dyn.h" },
	{ "lib/avltree_base.h", "lib/avltree_base.h" },
	{ "lib/avltree.h", "lib/avltree.h" },
	{ "lib/mempool.h", "lib/mempool.h" },
	{ "lib/dict.h", "lib/dict.h" },
	{ "lib/list.h", "lib/list.h" },
	{ "lib/value.h", "lib/value.h" },
	{ "lib/jnum.h", "lib/jnum.h" },
	{ "lib/json.h", "lib/json.h" },
	{ "lib/template.h", "lib/template.h" },
};


int g_FileCount = sizeof(g_Files) / sizeof(FileEntry);


void PrintBanner()
{
	printf("========================================\n");
	printf("    XRT Single Header File Maker\n");
	printf("========================================\n\n");
}


void PrintUsage()
{
	printf("Usage: single_head_maker [options]\n\n");
	printf("Options:\n");
	printf("  -o <output>   Output file path (default: xrt.h)\n");
	printf("  -s <source>   Source directory (default: ..)\n");
	printf("  -h            Show this help\n\n");
	printf("Example:\n");
	printf("  single_head_maker -o xrt.h\n\n");
}


char* ReadFileContent(const char* path)
{
	FILE* fp = fopen(path, "rb");
	if ( !fp ) {
		fprintf(stderr, "Error: Cannot open file '%s'\n", path);
		return NULL;
	}
	
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	char* content = (char*)malloc(size + 1);
	if ( !content ) {
		fclose(fp);
		fprintf(stderr, "Error: Memory allocation failed\n");
		return NULL;
	}
	
	size_t readSize = fread(content, 1, size, fp);
	if ( readSize != (size_t)size ) {
		free(content);
		fclose(fp);
		fprintf(stderr, "Error: Failed to read file '%s'\n", path);
		return NULL;
	}
	
	content[readSize] = '\0';
	fclose(fp);
	
	return content;
}


int ShouldSkipLine(const char* line, int isFirstFile)
{
	char trimmed[8192] = {0};
	const char* src = line;
	char* dst = trimmed;
	
	while ( *src == ' ' || *src == '\t' ) {
		src++;
	}
	
	while ( *src && *src != '\r' && *src != '\n' && dst < trimmed + sizeof(trimmed) - 1 ) {
		*dst++ = *src++;
	}
	*dst = '\0';
	
	if ( strncmp(trimmed, "#include", 8) == 0 ) {
		const char* includeTarget = trimmed + 8;
		while ( *includeTarget == ' ' || *includeTarget == '\t' ) {
			includeTarget++;
		}
		
		if ( *includeTarget == '"' ) {
			if ( strstr(includeTarget, "lib/") != NULL ) {
				return 1;
			}
			
			if ( strstr(includeTarget, "xrt.h") != NULL && !isFirstFile ) {
				return 1;
			}
		}
	}
	
	if ( strncmp(trimmed, "#ifndef XXRTL_CORE", 18) == 0 ) {
		return 1;
	}
	
	if ( strncmp(trimmed, "#define XXRTL_CORE", 18) == 0 ) {
		return 0;
	}
	
	if ( strstr(line, "XXRTL_CORE") != NULL && strncmp(trimmed, "#endif", 6) == 0 ) {
		return 1;
	}
	
	return 0;
}


int WriteHeader(FILE* out)
{
	time_t now = time(NULL);
	char timeStr[64];
	strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
	
	fprintf(out, "/*\n");
	fprintf(out, "\n");
	fprintf(out, "    XRT Single Header File\n");
	fprintf(out, "    Generated: %s\n", timeStr);
	fprintf(out, "\n");
	fprintf(out, "    MIT License\n");
	fprintf(out, "\n");
	fprintf(out, "    Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>\n");
	fprintf(out, "\n");
	fprintf(out, "    Permission is hereby granted, free of charge, to any person obtaining a copy\n");
	fprintf(out, "    of this software and associated documentation files (the \"Software\"), to deal\n");
	fprintf(out, "    in the Software without restriction, including without limitation the rights\n");
	fprintf(out, "    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n");
	fprintf(out, "    copies of the Software, and to permit persons to whom the Software is\n");
	fprintf(out, "    furnished to do so, subject to the following conditions:\n");
	fprintf(out, "\n");
	fprintf(out, "    The above copyright notice and this permission notice shall be included in all\n");
	fprintf(out, "    copies or substantial portions of the Software.\n");
	fprintf(out, "\n");
	fprintf(out, "    THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
	fprintf(out, "    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
	fprintf(out, "    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n");
	fprintf(out, "    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
	fprintf(out, "    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
	fprintf(out, "    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n");
	fprintf(out, "    SOFTWARE.\n");
	fprintf(out, "\n");
	fprintf(out, "*/\n");
	fprintf(out, "\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "// XRT Single Header File\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "//\n");
	fprintf(out, "// Usage:\n");
	fprintf(out, "//   #define XRT_IMPLEMENTATION\n");
	fprintf(out, "//   #include \"xrt.h\"\n");
	fprintf(out, "//\n");
	fprintf(out, "// Note:\n");
	fprintf(out, "//   - Define XRT_IMPLEMENTATION in exactly one source file\n");
	fprintf(out, "//   - Include this header in all other files\n");
	fprintf(out, "//   - No need to link against xrt library\n");
	fprintf(out, "//\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "\n");
	fprintf(out, "#ifndef XRT_SINGLE_HEADER\n");
	fprintf(out, "#define XRT_SINGLE_HEADER\n");
	fprintf(out, "\n");
	
	fprintf(out, "// ========================================\n");
	fprintf(out, "// Additional headers from xrt.c (must be included first)\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "\n");
	fprintf(out, "#if defined(_WIN32) || defined(_WIN64)\n");
	fprintf(out, "\t#ifdef __TINYC__\n");
	fprintf(out, "\t\t#include <winapi/shellapi.h>\n");
	fprintf(out, "\t\t#include <winapi/iphlpapi.h>\n");
	fprintf(out, "\t\t\n");
	fprintf(out, "\t\t// Declare function (load from kernel32.dll)\n");
	fprintf(out, "\t\tULONGLONG GetTickCount64();\n");
	fprintf(out, "\t#else\n");
	fprintf(out, "\t\t#include <shellapi.h>\n");
	fprintf(out, "\t\t#include <iphlpapi.h>\n");
	fprintf(out, "\t#endif\n");
	fprintf(out, "\t#include <sys/types.h>\n");
	fprintf(out, "\t#include <sys/stat.h>\n");
	fprintf(out, "\t#include <wchar.h>\n");
	fprintf(out, "\t#pragma comment (lib, \"shell32\")\n");
	fprintf(out, "\t#pragma comment (lib, \"Ws2_32\")\n");
	fprintf(out, "\t#pragma comment (lib, \"IPHLPAPI\")\n");
	fprintf(out, "#else\n");
	fprintf(out, "\t#include <fcntl.h>\n");
	fprintf(out, "\t#include <sys/stat.h>\n");
	fprintf(out, "\t#include <net/if.h>\n");
	fprintf(out, "\t#include <sys/ioctl.h>\n");
	fprintf(out, "\t#include <errno.h>\n");
	fprintf(out, "\t#include <wchar.h>\n");
	fprintf(out, "\t#include <netdb.h>\n");
	fprintf(out, "\t#include <dirent.h>\n");
	fprintf(out, "\t#include <sys/wait.h>\n");
	fprintf(out, "#endif\n");
	fprintf(out, "\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "// End of additional headers\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "\n");
	
	return 1;
}


int WriteFooter(FILE* out)
{
	fprintf(out, "\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "// End of Single Header\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "\n");
	fprintf(out, "#endif // XRT_SINGLE_HEADER\n");
	
	return 1;
}


int ProcessFile(FILE* out, const char* filePath, const char* fileName, int isFirstFile)
{
	char* content = ReadFileContent(filePath);
	if ( !content ) {
		return 0;
	}
	
	fprintf(out, "\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "// File: %s\n", fileName);
	fprintf(out, "// ========================================\n");
	fprintf(out, "\n");
	
	const char* lineStart = content;
	int lineCount = 0;
	
	while ( *lineStart ) {
		const char* lineEnd = lineStart;
		while ( *lineEnd && *lineEnd != '\r' && *lineEnd != '\n' ) {
			lineEnd++;
		}
		
		int lineLen = (int)(lineEnd - lineStart);
		char line[8192] = {0};
		strncpy(line, lineStart, lineLen);
		
		if ( !ShouldSkipLine(line, isFirstFile) ) {
			fwrite(line, 1, lineLen, out);
		} else {
			fprintf(out, "// %s\n", line);
		}
		
		fprintf(out, "\n");
		
		while ( *lineEnd == '\r' || *lineEnd == '\n' ) {
			lineEnd++;
		}
		
		lineStart = lineEnd;
		lineCount++;
	}
	
	free(content);
	
	return lineCount;
}


int MergeFiles(const char* sourceDir, const char* outputFile)
{
	FILE* out = fopen(outputFile, "wb");
	if ( !out ) {
		fprintf(stderr, "Error: Cannot create output file '%s'\n", outputFile);
		return 0;
	}
	
	if ( !WriteHeader(out) ) {
		fclose(out);
		return 0;
	}
	
	int totalLines = 0;
	int totalFiles = 0;
	
	for ( int i = 0; i < g_FileCount; i++ ) {
		char fullPath[MAX_PATH];
		sprintf(fullPath, "%s/%s", sourceDir, g_Files[i].path);
		
		int lines = ProcessFile(out, fullPath, g_Files[i].name, i == 0);
		if ( lines <= 0 ) {
			fclose(out);
			return 0;
		}
		
		totalLines += lines;
		totalFiles++;
		
		printf("  [%2d/%2d] Merged: %s (%d lines)\n", i + 1, g_FileCount, g_Files[i].name, lines);
	}
	
	if ( !WriteFooter(out) ) {
		fclose(out);
		return 0;
	}
	
	fclose(out);
	
	printf("\n");
	printf("✅ Successfully merged %d files\n", totalFiles);
	printf("   Total lines: %d\n", totalLines);
	printf("   Output: %s\n\n", outputFile);
	
	return 1;
}


int main(int argc, char* argv[])
{
	PrintBanner();
	
	char sourceDir[MAX_PATH] = "..";
	char outputFile[MAX_PATH] = "xrt.h";
	
	for ( int i = 1; i < argc; i++ ) {
		if ( strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ) {
			PrintUsage();
			return 0;
		}
		else if ( strcmp(argv[i], "-o") == 0 && i + 1 < argc ) {
			strcpy(outputFile, argv[++i]);
		}
		else if ( strcmp(argv[i], "-s") == 0 && i + 1 < argc ) {
			strcpy(sourceDir, argv[++i]);
		}
	}
	
	printf("Configuration:\n");
	printf("  Source: %s\n", sourceDir);
	printf("  Output: %s\n\n", outputFile);
	
	printf("Merging files...\n\n");
	
	int result = MergeFiles(sourceDir, outputFile);
	
	if ( result ) {
		printf("✅ Single header file generated successfully!\n\n");
		return 0;
	} else {
		printf("❌ Failed to generate single header file!\n\n");
		return 1;
	}
}
