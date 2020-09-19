/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod GameUILagFix
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include <sh_memory.h>

static struct SrcdsPatch
{
    const char* pSignature;
    const unsigned char* pPatchSignature;
    const char* pPatchPattern;
    const unsigned char* pPatch;

    unsigned char* pOriginal;
    uintptr_t pAddress;
    uintptr_t pPatchAddress;
    bool engine;
} gs_Patches[] = {
    // Removes FL_ONTRAIN flag for game_ui think
#ifdef PLATFORM_WINDOWS
    {
        "CGameUI::Think",
        (unsigned char*)"\x53\x8D\x9F\xD8\x00\x00\x00\x89\x45\xF8\x83\xC8\x10",
        "xxxxxxxxxxxxx",
        (unsigned char*)"\x53\x8D\x9F\xD8\x00\x00\x00\x89\x45\xF8\x90\x90\x90",
        0, 0, 0, false
    }
#elif defined PLATFORM_LINUX
    {
        "CGameUI::Think",
        (unsigned char*)"\xC7\x44\x24\x04\x10\x00\x00\x00\x89\x34\x24\xE8\x00\x00\x00\x00",
        "xxxxxxxxxxxx????",
        (unsigned char*)"\xC7\x44\x24\x04\x10\x00\x00\x00\x89\x34\x24\x90\x90\x90\x90\x90",
        0, 0, 0, false
    }
#endif
};

uintptr_t FindPattern(uintptr_t BaseAddr, const unsigned char* pData, const char* pPattern, size_t MaxSize);

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

GameUILagFix g_Extension;		/**< Global singleton for extension's main interface */
SMEXT_LINK(&g_Extension);

IGameConfig* g_pGameConf = NULL;

bool GameUILagFix::SDK_OnLoad(char* error, size_t maxlength, bool late)
{
    char conf_error[255] = "";
    if (!gameconfs->LoadGameConfigFile("GameUILagFix.games", &g_pGameConf, conf_error, sizeof(conf_error)))
    {
        if (conf_error[0])
            snprintf(error, maxlength, "Could not read GameUILagFix.games.txt: %s", conf_error);

        return false;

    }

    for (size_t i = 0; i < sizeof(gs_Patches) / sizeof(*gs_Patches); i++)
    {
        struct SrcdsPatch* pPatch = &gs_Patches[i];
        int PatchLen = strlen(pPatch->pPatchPattern);
        
        if (!g_pGameConf->GetMemSig(pPatch->pSignature, (void**)&pPatch->pAddress) || !pPatch->pAddress)
        {
            snprintf(error, maxlength, "Could not find symbol: %s", pPatch->pSignature);
            SDK_OnUnload();
            return false;
        }

        pPatch->pPatchAddress = FindPattern(pPatch->pAddress, pPatch->pPatchSignature, pPatch->pPatchPattern, 1024);
        if (!pPatch->pPatchAddress)
        {
            snprintf(error, maxlength, "Could not find patch signature for symbol: %s", pPatch->pSignature);
            SDK_OnUnload();
            return false;
        }

        pPatch->pOriginal = (unsigned char*)malloc(PatchLen * sizeof(unsigned char));

        SourceHook::SetMemAccess((void*)pPatch->pPatchAddress, PatchLen, SH_MEM_READ | SH_MEM_WRITE | SH_MEM_EXEC);
        for (int j = 0; j < PatchLen; j++)
        {
            pPatch->pOriginal[j] = *(unsigned char*)(pPatch->pPatchAddress + j);
            *(unsigned char*)(pPatch->pPatchAddress + j) = pPatch->pPatch[j];
        }
        SourceHook::SetMemAccess((void*)pPatch->pPatchAddress, PatchLen, SH_MEM_READ | SH_MEM_EXEC);
    }
    return true;
}

void GameUILagFix::SDK_OnUnload()
{
    gameconfs->CloseGameConfigFile(g_pGameConf);

    for (size_t i = 0; i < sizeof(gs_Patches) / sizeof(*gs_Patches); i++)
    {
        struct SrcdsPatch* pPatch = &gs_Patches[i];
        int PatchLen = strlen(pPatch->pPatchPattern);

        if (!pPatch->pOriginal)
            continue;

        SourceHook::SetMemAccess((void*)pPatch->pPatchAddress, PatchLen, SH_MEM_READ | SH_MEM_WRITE | SH_MEM_EXEC);
        for (int j = 0; j < PatchLen; j++)
        {
            *(unsigned char*)(pPatch->pPatchAddress + j) = pPatch->pOriginal[j];
        }
        SourceHook::SetMemAccess((void*)pPatch->pPatchAddress, PatchLen, SH_MEM_READ | SH_MEM_EXEC);

        free(pPatch->pOriginal);
        pPatch->pOriginal = NULL;
    }
}

uintptr_t FindPattern(uintptr_t BaseAddr, const unsigned char* pData, const char* pPattern, size_t MaxSize)
{
    unsigned char* pMemory;
        uintptr_t PatternLen = strlen(pPattern);

        pMemory = reinterpret_cast<unsigned char*>(BaseAddr);

        for (uintptr_t i = 0; i < MaxSize; i++)
        {
            uintptr_t Matches = 0;
            while (*(pMemory + i + Matches) == pData[Matches] || pPattern[Matches] != 'x')
            {
                Matches++;
                if (Matches == PatternLen)
                    return (uintptr_t)(pMemory + i);
            }
        }

    return 0x00;
}
