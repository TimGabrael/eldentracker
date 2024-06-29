#pragma once
#include "mem.h"
#include <string>


extern void(*ApplyDmgCallbackContinue)(ChrDamageModule* module, Instance* entity_instance, DamageStruct* damage, uint64_t param4, char param5);
void ApplyDmgCallbackFunction(ChrDamageModule* module, Instance* entity_instance, DamageStruct* damage, uint64_t param4, char param5);


std::string GetLocalPlayerData();

// attempt at recreating the internal function, that i end up using
// it doesn't really work, but i will leave it here just in case.
Vector2 WorldPositionToMapPosition(WorldMapLegacyConverter* converter, Vector3 pos, uint32_t special_map_id);
Vector2 WorldPositionToMapPosition(Vector3 pos, uint32_t map_id);
Vector2 CharacterMapPosition(Instance* instance_ptr);

