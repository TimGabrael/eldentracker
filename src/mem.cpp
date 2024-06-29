#include "mem.h"
#include "functionality.h"
#define NOMINMAX
#include <windows.h>
#include <Psapi.h>
#include <vector>
#include <iostream>
#include "logger.h"

const char* AobGameDataMan = "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3";
const char* AobGameMan = "48 8B 05 ?? ?? ?? ?? 80 B8 ?? ?? ?? ?? 0D 0F 94 C0 C3";
const char* AobFieldArea = "48 8B 3D ?? ?? ?? ?? 49 8B D8 48 8B F2 4C 8B F1 48 85 FF";
const char* AobMsgRepository = "48 8B 3D ?? ?? ?? ?? 44 0F B6 30 48 85 FF 75";
const char* AobWorldChrMan = "48 8B 0D ?? ?? ?? ?? 0F 28 74 24 20 48 83 C4 38";
const char* AobApplyDmgFunc = "4C 8B DC 55 53 56 57 41 56 41 57 49 8D 6B 88 48 81 EC 48 01 00 00";
const char* AobCSMenuMan = "48 8B 0D ?? ?? ?? ?? 48 8B 49 08 E8 ?? ?? ?? ?? 48 8B D0 48 8B CE";


using FnApplyEffect = void (*)(void* ChrIns, int spEffectId);
using FnRemoveEffect = void (*)(void* CSSpecialEffect, int spEffectId);
const char* AobApplyEffectFunc = "48 8B C4 48 89 58 08 48 89 70 10 57 48 81 EC ?? ?? ?? ?? 0F 28 05 ?? ?? ?? ?? 48 8B F1 0F 28 0D ?? ?? ?? ?? 48 8D 48 88";
const char* AobRemoveEffectFunc = "48 83 EC 28 8B C2 48 8B 51 08 48 85 D2 ?? ?? 90";


using FnSetEventFlag = void (*)(const uint64_t event_man, uint32_t* event_id, bool state);
using FnGetEventFlag = int (*)(void* manager, int flagID);
const char* AobEventFlagManagerFunc = "48 8B 3D ?? ?? ?? ?? 48 85 FF ?? ?? 32 C0 E9";
const char* AobEventFlagSetFunctionFunc = "?? ?? ?? ?? ?? 48 89 74 24 18 57 48 83 EC 30 48 8B DA 41 0F B6 F8 8B 12 48 8B F1 85 D2 0F 84 ?? ?? ?? ?? 45 84 C0"; // 5 to overwrite
const char* AobEventFlagGetFunctionFunc = "44 8B 41 ?? 44 8B DA 33 D2 41 8B C3 41 F7 F0";

struct PositionInformation {
    uint32_t map_id;
    Vector3 pos;
    float padding;
};

//struct WorldMapAreaConverter* converter = (struct WorldMapAreaConverter*)((uint8_t*)legacy_converter + 0x38);
// the pos_again is slightly different then pos, maybe it is the relative pos + the global pos or something similar
// for now i will just pass the same in there twice
// it seems like the function was designed to get the map positions of 2 instances at once in the same map_id
// but i will just call it twice
using FnWorldToMap = Vector2(*)(WorldMapAreaConverter*, Vector2& output, PositionInformation& pos, Vector3& pos_again);
// start_protected_game.exe + 0x875E60
FnWorldToMap PFUNC_WORLD_TO_MAP = 0;
const char* AobPfuncWorldToMap = "40 53 57 48 83 EC 68 48 8B 05 ?? ?? ?? ?? 48 33 C4";



static uintptr_t FindPattern(HMODULE module, const char* signature) {
    size_t signature_len = strnlen_s(signature, 1000);
    std::vector<uint8_t> pattern;
    std::vector<char> mask;
    pattern.reserve(signature_len);
    mask.reserve(signature_len);
    for(size_t i = 0; i < signature_len; i += 3) {
        uint8_t val = 0;
        bool valid = true;
        if(signature[i] >= 'A' && signature[i] <= 'F') {
            val = ((signature[i] - 'A') + 0xA) << 4;
        }
        else if(signature[i] >= '0' && signature[i] <= '9') {
            val = (signature[i] - '0') << 4;
        }
        else {
            valid = false;
        }
        if(signature[i+1] >= 'A' && signature[i+1] <= 'F') {
            val |= (signature[i+1] - 'A') + 0xA;
        }
        else if(signature[i+1] >= '0' && signature[i+1] <= '9') {
            val |= (signature[i+1] - '0');
        }
        else {
            valid = false;
        }
        pattern.push_back(val);
        mask.push_back(valid ? '#' : '?');
    }

	MODULEINFO module_info = {};
    if(GetModuleInformation(GetCurrentProcess(), module, &module_info, sizeof(MODULEINFO))) {
        uintptr_t base = reinterpret_cast<uintptr_t>(module_info.lpBaseOfDll);
        DWORD size =  module_info.SizeOfImage;

        for(uintptr_t i = 0; i < size - pattern.size(); i++) {
            bool found = true;
            for(size_t j = 0; j < pattern.size(); j++) {
                found &= mask[j] == '?' || pattern[j] == *(uint8_t*)(base + i + j);
            }
            if(found) {
                return base + i;
            }
        }
    }

	return 0;
}


static uintptr_t* GAME_DATA_MAN_PTR = 0;
static uintptr_t* GAME_MAN_PTR = 0;
static uintptr_t* FIELD_AREA_PTR = 0;
static uintptr_t* MSG_REPOSITORY_PTR = 0;
static uintptr_t* WORLD_CHR_MAN_PTR = 0;
static uintptr_t* CS_MENU_MAN_PTR = 0;

static uintptr_t APPLY_DMG_FUNC_PTR = 0;

struct Hook {
    uint8_t* start_loc = nullptr;
    uint8_t* tramp_loc = nullptr;

    uint32_t overwrite_size = 0;
    void SetV1(uint8_t* start, void* cb, uint32_t size) {
        this->overwrite_size = size;
        if(size < 5) {
            Log(string_format("Failed to Hook %p\nThe overwrite size is to small requires at least 5 bytes", (void*)start), LOG_LEVEL::LOG_ERROR);
            return;
        }
        uintptr_t cur_offset = 0x1000;
        while(!tramp_loc) {
            tramp_loc = (uint8_t*)VirtualAlloc(start + cur_offset, size + 0x100, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            cur_offset += 0x1000;
        }
        DWORD old_protection;
        if(!VirtualProtect(start, size, PAGE_EXECUTE_READWRITE, &old_protection)) {
            Log(string_format("Failed to Hook %p\nCouldn't change protection of the start address", (void*)start), LOG_LEVEL::LOG_ERROR);
            VirtualFree(this->tramp_loc, size + 0x100, MEM_RELEASE);
            return;
        }
        uint8_t jmp_instruction[] = {
            0xE9, 0x00, 0x00, 0x00, 0x00,
        };
        uint8_t tramp_instructions[] = {
            0xFF, 0x15, 0x02, 0x00, 0x00, 0x00,
            0xEB, 0x08,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        *(void**)(tramp_instructions + 8) = cb;
        memcpy(this->tramp_loc, start, size);
        memcpy(this->tramp_loc + size, tramp_instructions, sizeof(tramp_instructions));

        *(int*)(jmp_instruction + 1) = (int)(start - (this->tramp_loc + size + sizeof(tramp_instructions)));
        memcpy(this->tramp_loc + size + sizeof(tramp_instructions), jmp_instruction, sizeof(jmp_instruction));

        *(int*)(jmp_instruction + 1) = (int)(tramp_loc - start - sizeof(jmp_instruction));
        memset(start, 0x90, size);
        memcpy(start, jmp_instruction, sizeof(jmp_instruction));
    }
    void Set(uint8_t* start, void* cb, uint32_t size, void** out_func) {
        this->overwrite_size = size;
        if(size < 5) {
            Log(string_format("Failed to Hook %p\nThe overwrite size is to small requires at least 5 bytes", (void*)start), LOG_LEVEL::LOG_ERROR);
            return;
        }
        uintptr_t cur_offset = 0x1000;
        while(!tramp_loc) {
            tramp_loc = (uint8_t*)VirtualAlloc(start + cur_offset, size + 0x100, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            cur_offset += 0x1000;
        }
        DWORD old_protection;
        if(!VirtualProtect(start, size, PAGE_EXECUTE_READWRITE, &old_protection)) {
            Log(string_format("Failed to Hook %p\nCouldn't change protection of the start address", (void*)start), LOG_LEVEL::LOG_ERROR);
            VirtualFree(this->tramp_loc, size + 0x100, MEM_RELEASE);
            return;
        }
        uint8_t jmp_instruction[] = {
            0xE9, 0x00, 0x00, 0x00, 0x00,
        };
        uint8_t tramp_instructions[] = {
            0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        *(void**)(tramp_instructions + 6) = cb;
        memcpy(this->tramp_loc, tramp_instructions, sizeof(tramp_instructions));
        memcpy(this->tramp_loc + sizeof(tramp_instructions), start, size);


        *(int*)(jmp_instruction + 1) = (int)(start - (this->tramp_loc + size + sizeof(tramp_instructions)));
        memcpy(this->tramp_loc + size + sizeof(tramp_instructions), jmp_instruction, sizeof(jmp_instruction));


        *out_func = (void*)(this->tramp_loc + sizeof(tramp_instructions));
        *(int*)(jmp_instruction + 1) = (int)(tramp_loc - start - sizeof(jmp_instruction));
        memset(start, 0x90, size);
        memcpy(start, jmp_instruction, sizeof(jmp_instruction));
    }
    void Remove() {
        if(this->start_loc && this->tramp_loc && this->overwrite_size > 0) {
            memcpy(this->start_loc, this->tramp_loc, this->overwrite_size);
            VirtualFree(this->tramp_loc, this->overwrite_size + 0x100, MEM_RELEASE);
            this->tramp_loc = nullptr;
            this->start_loc = nullptr;
            this->overwrite_size = 0;
        }
    }
};


static uintptr_t* ExtractPtrFromInstruction(uintptr_t instr) {
    if(instr) {
        const uintptr_t offset = (uintptr_t)*(uint32_t*)(instr + 0x3);
        return (uintptr_t*)(instr + offset + 7);
    }
    else {
        return nullptr;
    }
}
void Mem_Initialize() {
    HMODULE handle = GetModuleHandleA("start_protected_game.exe");


    if(!GAME_DATA_MAN_PTR) {
        uintptr_t pattern = FindPattern(handle, AobGameDataMan);
        GAME_DATA_MAN_PTR = ExtractPtrFromInstruction(pattern);
        Log(string_format("GAME_DATA_MAN_PTR: %p", (void*)GAME_DATA_MAN_PTR), LOG_LEVEL::LOG_INFO);
    }

    if(!GAME_MAN_PTR) {
        uintptr_t pattern = FindPattern(handle, AobGameMan);
        GAME_MAN_PTR = ExtractPtrFromInstruction(pattern);
        Log(string_format("GAME_MAN_PTR: %p", (void*)GAME_MAN_PTR), LOG_LEVEL::LOG_INFO);
    }

    if(!FIELD_AREA_PTR) {
        uintptr_t pattern = FindPattern(handle, AobFieldArea);
        FIELD_AREA_PTR = ExtractPtrFromInstruction(pattern);
        Log(string_format("FIELD_AREA_PTR: %p", (void*)FIELD_AREA_PTR), LOG_LEVEL::LOG_INFO);
    }

    if(!MSG_REPOSITORY_PTR) {
        uintptr_t pattern = FindPattern(handle, AobMsgRepository);
        MSG_REPOSITORY_PTR = ExtractPtrFromInstruction(pattern);
        Log(string_format("MSG_REPOSITORY_PTR: %p", (void*)MSG_REPOSITORY_PTR), LOG_LEVEL::LOG_INFO);
    }

    if(!WORLD_CHR_MAN_PTR) {
        uintptr_t pattern = FindPattern(handle, AobWorldChrMan);
        WORLD_CHR_MAN_PTR = ExtractPtrFromInstruction(pattern);
        Log(string_format("WORLD_CHR_MAN_PTR: %p", (void*)WORLD_CHR_MAN_PTR), LOG_LEVEL::LOG_INFO);
    }

    if(!CS_MENU_MAN_PTR) {
        uintptr_t pattern = FindPattern(handle, AobCSMenuMan);
        CS_MENU_MAN_PTR = ExtractPtrFromInstruction(pattern);
        Log(string_format("CS_MENU_MAN: %p", (void*)CS_MENU_MAN_PTR), LOG_LEVEL::LOG_INFO);
    }

    if(!APPLY_DMG_FUNC_PTR) {
        APPLY_DMG_FUNC_PTR = FindPattern(handle, AobApplyDmgFunc);
        Log(string_format("APPLY_DMG_FUNC_PTR: %p", (void*)APPLY_DMG_FUNC_PTR), LOG_LEVEL::LOG_INFO);
        if(APPLY_DMG_FUNC_PTR) {
            static Hook hook;
            hook.Set((uint8_t*)APPLY_DMG_FUNC_PTR, (void*)&ApplyDmgCallbackFunction, 5, (void**)&ApplyDmgCallbackContinue);
        }
    }
    if(!PFUNC_WORLD_TO_MAP) {
        PFUNC_WORLD_TO_MAP = (FnWorldToMap)FindPattern(handle, AobPfuncWorldToMap);
        Log(string_format("PFUNC_WORLD_TO_MAP: %p", (void*)PFUNC_WORLD_TO_MAP), LOG_LEVEL::LOG_INFO);
    }

}

uint32_t Game_GetCurrentDeathCount() {
    if(GAME_DATA_MAN_PTR && *GAME_DATA_MAN_PTR) {
        return *(uint32_t*)(*GAME_DATA_MAN_PTR + 0x94);
    }
    return 0xFFFFFFFF;
}
uint32_t Game_GetCharacterCurrentHp(Instance* instance_ptr) {
    if(instance_ptr) {
        uint8_t* next = *(uint8_t**)((uint8_t*)instance_ptr + 0x190);
        if(next) {
            next = *(uint8_t**)(next);
            if(next) {
                return *(uint32_t*)(next + 0x138);
            }
        }
    }
    return 0;
}
uint32_t Game_GetCharacterMaxHp(Instance* instance_ptr) {
    if(instance_ptr) {
        uint8_t* next = *(uint8_t**)((uint8_t*)instance_ptr + 0x190);

        if(next) {
            next = *(uint8_t**)(next);
            if(next) {
                return *(uint32_t*)(next + 0x13C);
            }
        }
    }
    return 0;
}
Vector3 Game_GetCharacterPosition(Instance* instance_ptr) {
    if(instance_ptr) {
        if(Game_CharacterIsPlayer(instance_ptr)) {
            return *(Vector3*)((uint8_t*)instance_ptr + 0x6C0);
        }
        else {
            uint8_t* next = *(uint8_t**)((uint8_t*)instance_ptr + 0x190);
            if(next) {
                next = *(uint8_t**)(next + 0x68);
                if(next) {
                    return *(Vector3*)(next + 0x70);
                }
            }
        }
    }
    return {FLT_MAX, FLT_MAX, FLT_MAX};
}
uint32_t Game_DataGetCurrentHp(DataModule* module) {
    if(module) {
        return *(uint32_t*)((uint8_t*)module + 0x138);
    }
    return 0;
}
uint32_t Game_DataGetMaxHp(DataModule* module) {
    if(module) {
        return *(uint32_t*)((uint8_t*)module + 0x13C);
    }
    return 0;
}
uint32_t Game_GetNpcId(Instance* instance_ptr) {
    if(instance_ptr) {
        uint8_t* next = *(uint8_t**)((uint8_t*)instance_ptr + 0x28);
        if(next) {
            return *(uint32_t*)(next + 0x124);
        }
    }
    return 0;
}
uint32_t Game_GetCharacterMapId(Instance* instance_ptr) {
    if(instance_ptr) {
        if(Game_CharacterIsPlayer(instance_ptr)) {
            return *(uint32_t*)((uint8_t*)instance_ptr + 0x6D0);
        }
        else {
            return *(uint32_t*)((uint8_t*)instance_ptr + 0x0C);
        }
    }
    return 0xFFFFFFFF;
}
Instance* Game_GetInstanceFromDataModule(DataModule* module) {
    if(module) {
        return *(Instance**)((uint8_t*)module + 0x08);
    }
    return nullptr;
}
Instance* Game_GetLocalInstance() {
    if(WORLD_CHR_MAN_PTR && *WORLD_CHR_MAN_PTR) {
        uint8_t* next = *(uint8_t**)((uint8_t*)*WORLD_CHR_MAN_PTR + 0x10EF8);
        if(next) {
            return *(Instance**)(next + 0x0);
        }
    }
    return nullptr;
}
WorldMapLegacyConverter* Game_GetWorldMapLegacyConverter() {
    if(CS_MENU_MAN_PTR && *CS_MENU_MAN_PTR) {
        uint8_t* next = *(uint8_t**)((uint8_t*)*CS_MENU_MAN_PTR + 0x80);
        if(next) {
            next = *(uint8_t**)(next + 0x250);
            if(next) {
                return *(WorldMapLegacyConverter**)(next + 0x120);
            }
        }
    }
    return nullptr;
}
WorldMapAreaConverter* Game_GetWorldMapAreaConverter() {
    uint8_t* next = (uint8_t*)Game_GetWorldMapLegacyConverter();
    if(next) {
        return (WorldMapAreaConverter*)(next + 0x38);
    }
    return nullptr;
}
uint64_t Game_GetCharacterHandle(Instance* instance_ptr) {
    if(instance_ptr) {
        return *(uint64_t*)((uint8_t*)instance_ptr + 0x08);
    }
    return 0;
}
bool Game_CharacterIsPlayer(Instance* instance_ptr) {
    if(instance_ptr) {
        return 0xFFFFFFFF == *(uint32_t*)(((uint8_t*)instance_ptr + 0x0C));
    }
    return false;
}
Vector2 Game_GetMapSize() {
    if(GAME_DATA_MAN_PTR && *GAME_DATA_MAN_PTR) {
        uint8_t* next = *(uint8_t**)(*GAME_DATA_MAN_PTR + 0x68);
        if(next) {
            Vector2 map_size = {};
            map_size.x = *(float*)(next + 0x20);
            map_size.y = *(float*)(next + 0x24);
            return map_size;
        }
    }
    return {1.0f, 1.0f};
}
Vector2 Game_WorldToMap(Instance* instance_ptr) {
    if(instance_ptr) {
        PositionInformation pos_info {};
        pos_info.pos = Game_GetCharacterPosition(instance_ptr);
        pos_info.map_id = Game_GetCharacterMapId(instance_ptr);
        pos_info.padding = 0.0f;
        WorldMapAreaConverter* converter = Game_GetWorldMapAreaConverter();
        Vector3 another_pos = pos_info.pos;
        Vector2 output_2;
        Vector2 output_1 = PFUNC_WORLD_TO_MAP(converter, output_2, pos_info, another_pos);
        return output_1;
    }
    return {FLT_MAX, FLT_MAX};
}
Vector2 Game_GetNormalizedWorldToMap(Instance* instance_ptr) {
    if(instance_ptr) {
        Vector2 map_pos = Game_WorldToMap(instance_ptr);
        if(map_pos.x == FLT_MAX || map_pos.y == FLT_MAX) {
            return {FLT_MAX, FLT_MAX};
        }
        Vector2 map_size = Game_GetMapSize();
        map_pos.x /= map_size.x;
        map_pos.y /= map_size.y;
        return map_pos;
    }
    return {FLT_MAX, FLT_MAX};
}


