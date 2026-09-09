#include "../xwork_core/xwork_internal.h"

static const xwork_tool_def *xwork__get_builtin_tool_def(const char *sToolId)
{
    static const xwork_tool_def tFilesystemReadText = {
        XWORK_TOOL_FILESYSTEM_READ_TEXT,
        "Filesystem Read Text",
        "Read text from a workspace file.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_FILESYSTEM,
        XWORK_HOST_FILESYSTEM_READ_TEXT,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tFilesystemWriteText = {
        XWORK_TOOL_FILESYSTEM_WRITE_TEXT,
        "Filesystem Write Text",
        "Write text into a workspace file.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_FILESYSTEM,
        XWORK_HOST_FILESYSTEM_WRITE_TEXT,
        XWORK_SIDE_EFFECT_WORKSPACE_WRITE,
        XWORK_APPROVAL_ALWAYS,
        false
    };
    static const xwork_tool_def tFilesystemList = {
        XWORK_TOOL_FILESYSTEM_LIST,
        "Filesystem List",
        "List workspace directory entries.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_FILESYSTEM,
        XWORK_HOST_FILESYSTEM_LIST,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tFilesystemStat = {
        XWORK_TOOL_FILESYSTEM_STAT,
        "Filesystem Stat",
        "Read workspace file or directory metadata.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_FILESYSTEM,
        XWORK_HOST_FILESYSTEM_STAT,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tFilesystemGlob = {
        XWORK_TOOL_FILESYSTEM_GLOB,
        "Filesystem Glob",
        "Find workspace paths matching a glob pattern.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_FILESYSTEM,
        XWORK_HOST_FILESYSTEM_GLOB,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tFilesystemMkdir = {
        XWORK_TOOL_FILESYSTEM_MKDIR,
        "Filesystem Mkdir",
        "Create a workspace directory.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_FILESYSTEM,
        XWORK_HOST_FILESYSTEM_MKDIR,
        XWORK_SIDE_EFFECT_WORKSPACE_WRITE,
        XWORK_APPROVAL_ALWAYS,
        false
    };
    static const xwork_tool_def tFilesystemMove = {
        XWORK_TOOL_FILESYSTEM_MOVE,
        "Filesystem Move",
        "Move or rename a workspace path.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_FILESYSTEM,
        XWORK_HOST_FILESYSTEM_MOVE,
        XWORK_SIDE_EFFECT_WORKSPACE_WRITE,
        XWORK_APPROVAL_ALWAYS,
        false
    };
    static const xwork_tool_def tFilesystemDelete = {
        XWORK_TOOL_FILESYSTEM_DELETE,
        "Filesystem Delete",
        "Delete a workspace file or directory.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_FILESYSTEM,
        XWORK_HOST_FILESYSTEM_DELETE,
        XWORK_SIDE_EFFECT_WORKSPACE_WRITE,
        XWORK_APPROVAL_ALWAYS,
        false
    };
    static const xwork_tool_def tFilesystemApplyPatch = {
        XWORK_TOOL_FILESYSTEM_APPLY_PATCH,
        "Filesystem Apply Patch",
        "Apply a single-file workspace text patch.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_FILESYSTEM,
        XWORK_HOST_FILESYSTEM_APPLY_PATCH,
        XWORK_SIDE_EFFECT_WORKSPACE_WRITE,
        XWORK_APPROVAL_ALWAYS,
        false
    };
    static const xwork_tool_def tProcessExec = {
        XWORK_TOOL_PROCESS_EXEC,
        "Process Exec",
        "Run a process command and capture output.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_PROCESS,
        XWORK_HOST_PROCESS_EXEC,
        XWORK_SIDE_EFFECT_PROCESS_EXEC,
        XWORK_APPROVAL_ALWAYS,
        false
    };
    static const xwork_tool_def tProcessStartTerminal = {
        XWORK_TOOL_PROCESS_START_TERMINAL,
        "Process Start Terminal",
        "Start an interactive terminal-backed process session.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_PROCESS,
        XWORK_HOST_PROCESS_START_TERMINAL,
        XWORK_SIDE_EFFECT_PROCESS_EXEC,
        XWORK_APPROVAL_ALWAYS,
        false
    };
    static const xwork_tool_def tProcessListTerminals = {
        XWORK_TOOL_PROCESS_LIST_TERMINALS,
        "Process List Terminals",
        "List active interactive terminal sessions.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_PROCESS,
        XWORK_HOST_PROCESS_LIST_TERMINALS,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tProcessTerminalRead = {
        XWORK_TOOL_PROCESS_TERMINAL_READ,
        "Process Terminal Read",
        "Read ordered events from an interactive terminal session.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_PROCESS,
        XWORK_HOST_PROCESS_TERMINAL_READ,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tProcessTerminalWrite = {
        XWORK_TOOL_PROCESS_TERMINAL_WRITE,
        "Process Terminal Write",
        "Write text into an interactive terminal session.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_PROCESS,
        XWORK_HOST_PROCESS_TERMINAL_WRITE,
        XWORK_SIDE_EFFECT_PROCESS_EXEC,
        XWORK_APPROVAL_ALWAYS,
        false
    };
    static const xwork_tool_def tProcessTerminalResize = {
        XWORK_TOOL_PROCESS_TERMINAL_RESIZE,
        "Process Terminal Resize",
        "Resize an interactive terminal session.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_PROCESS,
        XWORK_HOST_PROCESS_TERMINAL_RESIZE,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tProcessTerminalStop = {
        XWORK_TOOL_PROCESS_TERMINAL_STOP,
        "Process Terminal Stop",
        "Stop and remove an interactive terminal session.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_PROCESS,
        XWORK_HOST_PROCESS_TERMINAL_STOP,
        XWORK_SIDE_EFFECT_PROCESS_EXEC,
        XWORK_APPROVAL_ALWAYS,
        false
    };
    static const xwork_tool_def tVcsStatus = {
        XWORK_TOOL_VCS_STATUS,
        "VCS Status",
        "Read version-control status for a workspace path.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_VCS,
        XWORK_HOST_VCS_STATUS,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tVcsDiff = {
        XWORK_TOOL_VCS_DIFF,
        "VCS Diff",
        "Read version-control diff for a workspace path.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_VCS,
        XWORK_HOST_VCS_DIFF,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tVcsLog = {
        XWORK_TOOL_VCS_LOG,
        "VCS Log",
        "Read recent version-control commit log for a workspace path.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_VCS,
        XWORK_HOST_VCS_LOG,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tVcsBranch = {
        XWORK_TOOL_VCS_BRANCH,
        "VCS Branch",
        "Read current version-control branch and dirty state for a workspace path.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_VCS,
        XWORK_HOST_VCS_BRANCH,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tEditorOpenBuffer = {
        XWORK_TOOL_EDITOR_OPEN_BUFFER,
        "Editor Open Buffer",
        "Open a workspace file as an editor buffer with optional selection.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_EDITOR,
        XWORK_HOST_EDITOR_OPEN_BUFFER,
        XWORK_SIDE_EFFECT_READ_ONLY,
        XWORK_APPROVAL_DEFAULT,
        false
    };
    static const xwork_tool_def tEditorApplyEdit = {
        XWORK_TOOL_EDITOR_APPLY_EDIT,
        "Editor Apply Edit",
        "Apply an edit to an open workspace editor buffer.",
        XWORK_TOOL_HOST_SERVICE,
        XWORK_HOST_EDITOR,
        XWORK_HOST_EDITOR_APPLY_EDIT,
        XWORK_SIDE_EFFECT_WORKSPACE_WRITE,
        XWORK_APPROVAL_ALWAYS,
        false
    };

    if ( !sToolId || !sToolId[0] ) {
        return NULL;
    }
    if ( strcmp(sToolId, XWORK_TOOL_FILESYSTEM_READ_TEXT) == 0 ) {
        return &tFilesystemReadText;
    }
    if ( strcmp(sToolId, XWORK_TOOL_FILESYSTEM_WRITE_TEXT) == 0 ) {
        return &tFilesystemWriteText;
    }
    if ( strcmp(sToolId, XWORK_TOOL_FILESYSTEM_LIST) == 0 ) {
        return &tFilesystemList;
    }
    if ( strcmp(sToolId, XWORK_TOOL_FILESYSTEM_STAT) == 0 ) {
        return &tFilesystemStat;
    }
    if ( strcmp(sToolId, XWORK_TOOL_FILESYSTEM_GLOB) == 0 ) {
        return &tFilesystemGlob;
    }
    if ( strcmp(sToolId, XWORK_TOOL_FILESYSTEM_MKDIR) == 0 ) {
        return &tFilesystemMkdir;
    }
    if ( strcmp(sToolId, XWORK_TOOL_FILESYSTEM_MOVE) == 0 ) {
        return &tFilesystemMove;
    }
    if ( strcmp(sToolId, XWORK_TOOL_FILESYSTEM_DELETE) == 0 ) {
        return &tFilesystemDelete;
    }
    if ( strcmp(sToolId, XWORK_TOOL_FILESYSTEM_APPLY_PATCH) == 0 ) {
        return &tFilesystemApplyPatch;
    }
    if ( strcmp(sToolId, XWORK_TOOL_PROCESS_EXEC) == 0 ) {
        return &tProcessExec;
    }
    if ( strcmp(sToolId, XWORK_TOOL_PROCESS_START_TERMINAL) == 0 ) {
        return &tProcessStartTerminal;
    }
    if ( strcmp(sToolId, XWORK_TOOL_PROCESS_LIST_TERMINALS) == 0 ) {
        return &tProcessListTerminals;
    }
    if ( strcmp(sToolId, XWORK_TOOL_PROCESS_TERMINAL_READ) == 0 ) {
        return &tProcessTerminalRead;
    }
    if ( strcmp(sToolId, XWORK_TOOL_PROCESS_TERMINAL_WRITE) == 0 ) {
        return &tProcessTerminalWrite;
    }
    if ( strcmp(sToolId, XWORK_TOOL_PROCESS_TERMINAL_RESIZE) == 0 ) {
        return &tProcessTerminalResize;
    }
    if ( strcmp(sToolId, XWORK_TOOL_PROCESS_TERMINAL_STOP) == 0 ) {
        return &tProcessTerminalStop;
    }
    if ( strcmp(sToolId, XWORK_TOOL_VCS_STATUS) == 0 ) {
        return &tVcsStatus;
    }
    if ( strcmp(sToolId, XWORK_TOOL_VCS_DIFF) == 0 ) {
        return &tVcsDiff;
    }
    if ( strcmp(sToolId, XWORK_TOOL_VCS_LOG) == 0 ) {
        return &tVcsLog;
    }
    if ( strcmp(sToolId, XWORK_TOOL_VCS_BRANCH) == 0 ) {
        return &tVcsBranch;
    }
    if ( strcmp(sToolId, XWORK_TOOL_EDITOR_OPEN_BUFFER) == 0 ) {
        return &tEditorOpenBuffer;
    }
    if ( strcmp(sToolId, XWORK_TOOL_EDITOR_APPLY_EDIT) == 0 ) {
        return &tEditorApplyEdit;
    }
    return NULL;
}

xwork_status xwork_runtime_register_tool(
    xwork_runtime *pRuntime,
    const xwork_tool_def *pDef
)
{
    xwork_tool_record *pTool;

    if ( !pRuntime || !pDef || !pDef->sToolId || !pDef->sToolId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pDef->eHostService != XWORK_HOST_NONE &&
         (!pDef->sOperationId || !pDef->sOperationId[0]) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( xwork_runtime_find_tool(pRuntime, pDef->sToolId) ) {
        return XWORK_ERROR_ALREADY_EXISTS;
    }

    pTool = (xwork_tool_record *)calloc(1u, sizeof(*pTool));
    if ( !pTool ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pTool->sToolId = xwork__dup_cstr(pDef->sToolId);
    pTool->sDisplayName = xwork__dup_cstr(pDef->sDisplayName);
    pTool->sDescription = xwork__dup_cstr(pDef->sDescription);
    if ( !pTool->sToolId ) {
        free(pTool);
        return XWORK_ERROR_NO_MEMORY;
    }

    pTool->tDef = *pDef;
    pTool->tDef.sToolId = pTool->sToolId;
    pTool->tDef.sDisplayName = pTool->sDisplayName;
    pTool->tDef.sDescription = pTool->sDescription;

    pTool->pNext = pRuntime->pTools;
    pRuntime->pTools = pTool;
    return XWORK_OK;
}

const xwork_tool_def *xwork_runtime_find_tool(
    const xwork_runtime *pRuntime,
    const char *sToolId
)
{
    xwork_tool_record *pCursor;

    if ( !pRuntime || !sToolId || !sToolId[0] ) {
        return NULL;
    }

    for ( pCursor = pRuntime->pTools; pCursor; pCursor = pCursor->pNext ) {
        if ( pCursor->sToolId && strcmp(pCursor->sToolId, sToolId) == 0 ) {
            return &pCursor->tDef;
        }
    }
    return NULL;
}

const xwork_tool_def *xwork_get_builtin_tool_def(const char *sToolId)
{
    return xwork__get_builtin_tool_def(sToolId);
}

xwork_status xwork_runtime_register_builtin_tool(
    xwork_runtime *pRuntime,
    const char *sToolId
)
{
    const xwork_tool_def *pDef = xwork__get_builtin_tool_def(sToolId);

    if ( !pDef ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    return xwork_runtime_register_tool(pRuntime, pDef);
}
