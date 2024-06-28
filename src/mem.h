#pragma once
#include <stdint.h>

void Mem_Initialize();

struct Instance;
struct DataModule;

struct DamageStruct {
    char unk[0x228];
    int damage;
};
struct ChrDamageModule {
    char unk[0x8];
    Instance* instance;
};
struct Vector3 {
    float x;
    float y; // y is up
    float z;
};
struct Vector2 {
    float x;
    float y;
};
struct WorldMapLegacyConverter;
struct WorldMapAreaConverter;

uint32_t Game_GetCurrentDeathCount();

// either enemy instance or player instance
// both work
uint32_t Game_GetCharacterCurrentHp(Instance* instance_ptr);
uint32_t Game_GetCharacterMaxHp(Instance* instance_ptr);
Vector3 Game_GetCharacterPosition(Instance* instance_ptr);
uint32_t Game_DataGetCurrentHp(DataModule* module);
uint32_t Game_DataGetMaxHp(DataModule* module);
uint32_t Game_GetNpcId(Instance* instance_ptr);
uint32_t Game_GetCharacterMapId(Instance* instance_ptr);
Instance* Game_GetInstanceFromDataModule(DataModule* module);
Instance* Game_GetLocalInstance();
WorldMapLegacyConverter* Game_GetWorldMapLegacyConverter();
WorldMapAreaConverter* Game_GetWorldMapAreaConverter();
uint64_t Game_GetCharacterHandle(Instance* instance_ptr);

bool Game_CharacterIsPlayer(Instance* instance_ptr);

Vector2 Game_GetMapSize();
Vector2 Game_WorldToMap(Instance* instance_ptr);
Vector2 Game_GetNormalizedWorldToMap(Instance* instance_ptr);



