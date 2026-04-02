
#define XRT_NO_THREAD
#define XRT_NO_QUEUE
#define XRT_NO_COROUTINE
#define XRT_NO_NETWORK
#define XRT_NO_CRYPTO
#define XRT_NO_NETTLS
#define XRT_NO_XID
#define XRT_NO_ARRAY
#define XRT_NO_BSMN
#define XRT_NO_MEMUNIT
#define XRT_NO_MEMPOOL_FS
#define XRT_NO_STACK
#define XRT_NO_AVLTREE
#define XRT_NO_MEMPOOL
#define XRT_NO_DICT
#define XRT_NO_LIST
#define XRT_NO_VALUE
#define XRT_NO_JSON
#define XRT_NO_XSON
#define XRT_NO_TEMPLATE
#define XRT_NO_REGEX
#define XRT_NO_FILE_ASYNC
#define XRT_NO_SUBPROCESS

#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"


#define MAKER_MAX_PATH 4096
#define MAX_FILES 100


typedef struct {
	char path[MAKER_MAX_PATH];
	char* name;
} FileEntry;


int FileExists(const char* path)
{
	FILE* fp = fopen(path, "rb");
	if ( fp ) {
		fclose(fp);
		return 1;
	}
	return 0;
}


str FindXrtCRecursive(str startPath)
{
	if ( !startPath || startPath[0] == '\0' ) {
		return xCore.sNull;
	}
	
	char path[MAKER_MAX_PATH];
	strcpy(path, startPath);
	
	int maxDepth = 20;
	for ( int depth = 0; depth < maxDepth; depth++ ) {
		char xrtCPath[MAKER_MAX_PATH];
		strcpy(xrtCPath, path);
		strcat(xrtCPath, "/xrt.c");
		
		if ( FileExists(xrtCPath) ) {
			str result = xrtCopyStr(xrtCPath, 0);
			return result;
		}
		
		str parentDir = xrtPathGetDir(path, strlen(path));
		if ( !parentDir || parentDir[0] == '\0' || strcmp(parentDir, path) == 0 ) {
			break;
		}
		
		strcpy(path, parentDir);
	}
	
	return xCore.sNull;
}


void AutoDetectPaths(char* xrtCPath, char* outputFile)
{
	str exePath = xCore.AppFile;
	str exeDir = xrtPathGetDir(exePath, strlen(exePath));
	
	str foundXrtC = FindXrtCRecursive(exeDir);
	
	if ( foundXrtC != xCore.sNull ) {
		strcpy(xrtCPath, foundXrtC);
		
		str xrtDir = xrtPathGetDir(foundXrtC, strlen(foundXrtC));
		strcpy(outputFile, xrtDir);
		strcat(outputFile, "/singlehead/xrt.h");
		
		printf("Auto-detected paths:\n");
		printf("  xrt.c: %s\n", xrtCPath);
		printf("  Output: %s\n", outputFile);
	} else {
		fprintf(stderr, "Error: Could not find xrt.c file\n");
		fprintf(stderr, "Searched starting from: %s\n", exeDir);
		xrtUnit();
		exit(1);
	}
}


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
	printf("  -o <output>   Output file path (default: auto-detect)\n");
	printf("  -c <xrt.c>    Path to xrt.c file (default: auto-detect)\n");
	printf("  -s <source>   Source directory for includes (default: auto-detect)\n");
	printf("  -h            Show this help\n\n");
	printf("Examples:\n");
	printf("  single_head_maker\n");
	printf("  single_head_maker -o xrt.h -c ../xrt.c\n\n");
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

	if ( readSize >= 3 &&
		(unsigned char)content[0] == 0xEF &&
		(unsigned char)content[1] == 0xBB &&
		(unsigned char)content[2] == 0xBF ) {
		memmove(content, content + 3, readSize - 2);
	}
	
	return content;
}


char g_ProcessedFiles[MAX_FILES][MAKER_MAX_PATH];
int g_ProcessedCount = 0;


int IsFileProcessed(const char* path)
{
	for ( int i = 0; i < g_ProcessedCount; i++ ) {
		if ( strcmp(g_ProcessedFiles[i], path) == 0 ) {
			return 1;
		}
	}
	return 0;
}


void AddProcessedFile(const char* path)
{
	if ( g_ProcessedCount < MAX_FILES ) {
		strcpy(g_ProcessedFiles[g_ProcessedCount++], path);
	}
}


void NormalizePath(char* path)
{
	char* p = path;
	while ( *p ) {
		if ( *p == '\\' ) {
			*p = '/';
		}
		p++;
	}
}


void ResolvePath(const char* baseFile, const char* includePath, char* result, const char* sourceDir)
{
	if ( includePath[0] == '/' || (includePath[0] && includePath[1] == ':') ) {
		strcpy(result, includePath);
	} else {
		char baseDir[MAKER_MAX_PATH];
		strcpy(baseDir, baseFile);
		
		char* lastSlash = strrchr(baseDir, '/');
		if ( lastSlash ) {
			*lastSlash = '\0';
			sprintf(result, "%s/%s", baseDir, includePath);
		} else {
			sprintf(result, "%s/%s", sourceDir, includePath);
		}
	}
	
	NormalizePath(result);
}


int ShouldSkipInclude(const char* includePath, const char* skipInclude)
{
	if ( !skipInclude || skipInclude[0] == '\0' ) {
		return 0;
	}
	
	char normalizedInclude[MAKER_MAX_PATH];
	strcpy(normalizedInclude, includePath);
	NormalizePath(normalizedInclude);
	
	char normalizedSkip[MAKER_MAX_PATH];
	strcpy(normalizedSkip, skipInclude);
	NormalizePath(normalizedSkip);
	
	char* lastSlash = strrchr(normalizedSkip, '/');
	if ( lastSlash ) {
		if ( strcmp(normalizedInclude, lastSlash + 1) == 0 ) {
			return 1;
		}
	} else {
		if ( strcmp(normalizedInclude, normalizedSkip) == 0 ) {
			return 1;
		}
	}
	return 0;
}


void ProcessIncludes(FILE* out, const char* filePath, int depth, int* totalLines, const char* sourceDir, const char* skipInclude)
{
	if ( depth > 50 ) {
		fprintf(stderr, "Error: Include depth too deep (possible circular reference)\n");
		return;
	}

	char normalizedPath[MAKER_MAX_PATH];
	strcpy(normalizedPath, filePath);
	NormalizePath(normalizedPath);

	if ( IsFileProcessed(normalizedPath) ) {
		fprintf(out, "// (skipped duplicate include: %s)\n", filePath);
		return;
	}
	AddProcessedFile(normalizedPath);

	char* content = ReadFileContent(filePath);
	if ( !content ) {
		return;
	}
	
	fprintf(out, "\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "// File: %s\n", filePath);
	fprintf(out, "// ========================================\n");
	fprintf(out, "\n");
	
	const char* lineStart = content;
	
	while ( *lineStart ) {
		const char* lineEnd = lineStart;
		while ( *lineEnd && *lineEnd != '\r' && *lineEnd != '\n' ) {
			lineEnd++;
		}
		
		int lineLen = (int)(lineEnd - lineStart);
		char line[8192] = {0};
		strncpy(line, lineStart, lineLen);
		
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
				includeTarget++;
				char includePath[MAKER_MAX_PATH] = {0};
				char* p = includePath;
				while ( *includeTarget && *includeTarget != '"' && p < includePath + MAKER_MAX_PATH - 1 ) {
					*p++ = *includeTarget++;
				}
				*p = '\0';

				if ( ShouldSkipInclude(includePath, skipInclude) ) {
					fprintf(out, "// (skipped include: %s)\n", line);
				} else {
					char fullPath[MAKER_MAX_PATH];
					ResolvePath(filePath, includePath, fullPath, sourceDir);
					
					ProcessIncludes(out, fullPath, depth + 1, totalLines, sourceDir, skipInclude);
				}
			} else {
				fwrite(line, 1, lineLen, out);
				fprintf(out, "\n");
				(*totalLines)++;
			}
		} else {
			fwrite(line, 1, lineLen, out);
			fprintf(out, "\n");
			(*totalLines)++;
		}

		while ( *lineEnd == '\r' || *lineEnd == '\n' ) {
			lineEnd++;
		}
		
		lineStart = lineEnd;
	}
	
	free(content);
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
	fprintf(out, "    in Software without restriction, including without limitation the rights\n");
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
	fprintf(out, "//   // In exactly ONE source file:\n");
	fprintf(out, "//   #define XRT_IMPLEMENTATION\n");
	fprintf(out, "//   #include \"xrt.h\"\n");
	fprintf(out, "//\n");
	fprintf(out, "//   // In all other files:\n");
	fprintf(out, "//   #include \"xrt.h\"\n");
	fprintf(out, "//\n");
	fprintf(out, "// Note:\n");
	fprintf(out, "//   - Define XRT_IMPLEMENTATION in exactly one source file\n");
	fprintf(out, "//   - Include this header in all other files without XRT_IMPLEMENTATION\n");
	fprintf(out, "//   - No need to link against xrt library\n");
	fprintf(out, "//\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "\n");
	fprintf(out, "#ifndef XRT_SINGLE_HEADER\n");
	fprintf(out, "#define XRT_SINGLE_HEADER\n");
	fprintf(out, "\n");
	
	return 1;
}


int WriteImplementationHeader(FILE* out)
{
	fprintf(out, "\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "// Implementation\n");
	fprintf(out, "// ========================================\n");
	fprintf(out, "\n");
	fprintf(out, "#ifdef XRT_IMPLEMENTATION\n");
	fprintf(out, "\n");
	
	return 1;
}


int WriteImplementationFooter(FILE* out)
{
	fprintf(out, "\n");
	fprintf(out, "#endif // XRT_IMPLEMENTATION\n");
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


int BuildDeclOutputPath(const char* outputFile, char* declOutputFile)
{
	const char* sLastSlash;
	const char* sLastDot;
	size_t iDirLen;
	size_t iBaseLen;

	if ( outputFile == NULL || declOutputFile == NULL ) {
		return 0;
	}

	sLastSlash = strrchr(outputFile, '/');
	if ( sLastSlash == NULL ) {
		sLastSlash = strrchr(outputFile, '\\');
	}
	sLastDot = strrchr(outputFile, '.');
	if ( sLastDot == NULL || (sLastSlash && sLastDot < sLastSlash) ) {
		sLastDot = outputFile + strlen(outputFile);
	}

	iDirLen = sLastSlash ? (size_t)(sLastSlash - outputFile + 1) : 0;
	iBaseLen = (size_t)(sLastDot - (sLastSlash ? (sLastSlash + 1) : outputFile));

	memcpy(declOutputFile, outputFile, iDirLen);
	memcpy(declOutputFile + iDirLen, sLastSlash ? (sLastSlash + 1) : outputFile, iBaseLen);
	strcpy(declOutputFile + iDirLen + iBaseLen, "_decl.h");
	return 1;
}


int MergeFiles(const char* xrtCPath, const char* sourceDir, const char* outputFile)
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
	
	g_ProcessedCount = 0;
	int totalLines = 0;
	
	char xrtHPath[MAKER_MAX_PATH];
	ResolvePath(xrtCPath, "xrt.h", xrtHPath, sourceDir);
	
	printf("Processing header file...\n");
	ProcessIncludes(out, xrtHPath, 0, &totalLines, sourceDir, NULL);
	printf("  Header processed: %d lines\n\n", totalLines);
	
	if ( !WriteImplementationHeader(out) ) {
		fclose(out);
		return 0;
	}

	g_ProcessedCount = 0;
	int implLines = 0;

	printf("Processing implementation file...\n");
	ProcessIncludes(out, xrtCPath, 0, &implLines, sourceDir, "xrt.h");
	printf("  Implementation processed: %d lines\n", implLines);
	printf("  Files processed: %d\n\n", g_ProcessedCount);

	if ( !WriteImplementationFooter(out) ) {
		fclose(out);
		return 0;
	}

	if ( !WriteFooter(out) ) {
		fclose(out);
		return 0;
	}

	fclose(out);

	totalLines += implLines;

	printf("✅ Successfully generated single header file\n");
	printf("   Total lines: %d (header: %d, implementation: %d)\n", totalLines, totalLines - implLines, implLines);
	printf("   Output: %s\n\n", outputFile);
	
	return 1;
}


int MergeHeaderOnlyFile(const char* xrtCPath, const char* sourceDir, const char* outputFile)
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

	g_ProcessedCount = 0;
	{
		int totalLines = 0;
		char xrtHPath[MAKER_MAX_PATH];
		ResolvePath(xrtCPath, "xrt.h", xrtHPath, sourceDir);

		printf("Processing declaration-only header...\n");
		ProcessIncludes(out, xrtHPath, 0, &totalLines, sourceDir, NULL);
		printf("  Declaration header processed: %d lines\n", totalLines);
	}

	if ( !WriteFooter(out) ) {
		fclose(out);
		return 0;
	}

	fclose(out);
	printf("Declaration-only header output: %s\n\n", outputFile);
	return 1;
}


int main(int argc, char* argv[])
{
	xrtInit();
	
	PrintBanner();
	
	char sourceDir[MAKER_MAX_PATH] = "";
	char xrtCPath[MAKER_MAX_PATH] = "";
	char outputFile[MAKER_MAX_PATH] = "";
	char declOutputFile[MAKER_MAX_PATH] = "";
	
	int useAutoDetect = 1;
	
	for ( int i = 1; i < argc; i++ ) {
		if ( strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ) {
			PrintUsage();
			xrtUnit();
			return 0;
		}
		else if ( strcmp(argv[i], "-o") == 0 && i + 1 < argc ) {
			strcpy(outputFile, argv[++i]);
			useAutoDetect = 0;
		}
		else if ( strcmp(argv[i], "-c") == 0 && i + 1 < argc ) {
			strcpy(xrtCPath, argv[++i]);
			useAutoDetect = 0;
		}
		else if ( strcmp(argv[i], "-s") == 0 && i + 1 < argc ) {
			strcpy(sourceDir, argv[++i]);
			useAutoDetect = 0;
		}
	}
	
	if ( useAutoDetect ) {
		printf("No arguments provided, auto-detecting paths...\n\n");
		AutoDetectPaths(xrtCPath, outputFile);
		
		strcpy(sourceDir, xrtPathGetDir(xrtCPath, strlen(xrtCPath)));
	}
	
	printf("Configuration:\n");
	printf("  xrt.c: %s\n", xrtCPath);
	printf("  Source: %s\n", sourceDir);
	printf("  Output: %s\n\n", outputFile);

	if ( !BuildDeclOutputPath(outputFile, declOutputFile) ) {
		xrtUnit();
		return 1;
	}
	
	int result = MergeFiles(xrtCPath, sourceDir, outputFile);
	if ( result ) {
		result = MergeHeaderOnlyFile(xrtCPath, sourceDir, declOutputFile);
	}

	if ( result ) {
		printf("Single header files generated successfully.\n");
		printf("  Full: %s\n", outputFile);
		printf("  Decl: %s\n\n", declOutputFile);
		xrtUnit();
		return 0;
	}

	printf("Failed to generate single header file.\n\n");
	xrtUnit();
	return 1;
	
	if ( result ) {
		printf("✅ Single header file generated successfully!\n\n");
		xrtUnit();
		return 0;
	} else {
		printf("❌ Failed to generate single header file!\n\n");
		xrtUnit();
		return 1;
	}
}
