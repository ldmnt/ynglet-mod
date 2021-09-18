#include "pch-il2cpp.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include "il2cpp-appdata.h"
#include "helpers.h"
#include <strsafe.h>
#include <unordered_map>
#include <vector>

using namespace app;

bool hackActive = true;

const BYTE jumpTemplate[] = { 0x49, 0xbb, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x41, 0xff, 0xe3 };
const int jumpTemplateSize = 13;
const BYTE shortJumpTemplate[] = { 0xeb, 0x0 };
const BYTE shortJumpTemplateSize = 2;

typedef void(__fastcall* spawnFunction)(ShardSpawner*, ShardSpawner_SpawnSound__Enum, MethodInfo*);
typedef bool(__fastcall* onCollectFunction)(Triangle*, MethodInfo*);
typedef void(__fastcall* onSceneLoadedFunction)(PersistentManager*, Scene, LoadSceneMode__Enum, MethodInfo*);
spawnFunction ShardSpawner_Spawn_Trampoline;
onCollectFunction Triangle_OnCollect_Trampoline;
onSceneLoadedFunction PersistentManager_OnSceneLoaded_Trampoline;

std::unordered_map<PlayObjectBase*, std::vector<Shard*>> shardsOfTriangle;
std::unordered_map<Shard*, PlayObjectBase*> triangleOfShard;

void __fastcall ShardSpawner_Spawn_Modified(ShardSpawner* __this, ShardSpawner_SpawnSound__Enum spawnSound, MethodInfo* method) {
  if (hackActive) {
    PlayObjectBase* spawned = __this->fields.spawned;
    if (Object_1_op_Inequality((Object_1*)spawned, NULL, NULL)) {
      PlayObjectBase_Erase(spawned, NULL);
      __this->fields.spawned = (PlayObjectBase*)NULL;
    }
  }

  ShardSpawner_Spawn_Trampoline(__this, spawnSound, method);

  __this->fields.triangleHintArrow = (TriangleHintArrow*)NULL;

  if (hackActive) {
    // store in memory which shards are associated with the spawned triangle.
    // (will be used to control shard respawns)
    PlayObjectBase* spawned = __this->fields.spawned;
    List_1_Ynglet_Shard_* collectedShards = __this->fields.collectedShards;
    std::vector<Shard*> shards(collectedShards->fields._size);
    for (int i = 0; i < collectedShards->fields._size; ++i) {
      Shard* shard = (collectedShards->fields._items)->vector[i];
      shards[i] = shard;
      triangleOfShard[shard] = spawned;
    }
    shardsOfTriangle[spawned] = std::move(shards);
  }
}

bool __fastcall Triangle_OnCollect_Modified(Triangle* __this, MethodInfo* method) {
  if (hackActive) {
    // respawn all shards associated with the collected triangle
    for (Shard* shard : shardsOfTriangle[(PlayObjectBase*)__this]) {
      triangleOfShard.erase(shard);
    }
    shardsOfTriangle.erase((PlayObjectBase*)__this);
  }

  return Triangle_OnCollect_Trampoline(__this, method);
}

bool __fastcall ShouldShardRespawn(YngletPlayer* player, MethodInfo* method, Shard* shard) {
  return YngletPlayer_get_isInsideBubble(player, method) && !triangleOfShard.count(shard);
}

void __fastcall PersistentManager_OnSceneLoaded_Modified(PersistentManager* __this, Scene scene, LoadSceneMode__Enum mode, MethodInfo* method) {
  triangleOfShard.clear();
  shardsOfTriangle.clear();
  PersistentManager_OnSceneLoaded_Trampoline(__this, scene, mode, method);
}

void WriteAbsoluteJump(BYTE* src, DWORD64 tgt) {
  memcpy_s(src, jumpTemplateSize, jumpTemplate, jumpTemplateSize);
  *(DWORD64*)(src + 2) = tgt;
}

void WriteShortJump(BYTE* src, int off) {
  memcpy_s(src, shortJumpTemplateSize, shortJumpTemplate, shortJumpTemplateSize);
  *(BYTE*)(src + 1) = (signed char)(off - 2);
}

BYTE* Detour(BYTE* src, int hookLength, const BYTE* detourCode, int detourCodeLength) {
  DWORD oldProtection;
  VirtualProtect((LPVOID)src, hookLength, PAGE_EXECUTE_READWRITE, &oldProtection);

  BYTE* dst = (BYTE*)VirtualAlloc(NULL, detourCodeLength + hookLength + jumpTemplateSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (detourCode)
    memcpy_s(dst, detourCodeLength, detourCode, detourCodeLength);
  memcpy_s(dst + detourCodeLength, hookLength, src, hookLength);
  WriteAbsoluteJump(dst + detourCodeLength + hookLength, (DWORD64)src + hookLength);
  memset(src, 0x90, hookLength);
  WriteAbsoluteJump(src, (DWORD64)dst);

  VirtualProtect(src, hookLength, oldProtection, &oldProtection);
  return dst;
}

BYTE* Trampoline(BYTE* src, BYTE* dst, int len) {
  DWORD oldProtection;
  VirtualProtect((LPVOID)src, len, PAGE_EXECUTE_READWRITE, &oldProtection);

  BYTE* gate = (BYTE*)VirtualAlloc(NULL, len + jumpTemplateSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  memcpy_s(gate, len, src, len);
  WriteAbsoluteJump(gate + len, (uintptr_t)src + len);
  memset(src, 0x90, len);
  WriteAbsoluteJump(src, (DWORD64)dst);

  VirtualProtect(src, len, oldProtection, &oldProtection);
  return gate;
}

BYTE* TriangleOnCollectTrampoline(BYTE* src, BYTE* dst) {
  // Triangle.OnCollect starts with an instruction that contains an offset. A 13-byte absolute jump
  // would be a headache to set up without ruining the offset. As an easier workaround, we use only
  // 2 bytes to jump back 13 bytes, and we write our hook in these 13 unused bytes before the start of
  // the method.
  DWORD oldProtection;
  VirtualProtect((LPVOID)(src - 13), 15, PAGE_EXECUTE_READWRITE, &oldProtection);

  BYTE* gate = (BYTE*)VirtualAlloc(NULL, jumpTemplateSize + 2, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  memcpy_s(gate, 2, src, 2);
  WriteAbsoluteJump(gate + 2, (uintptr_t)src + 2);
  WriteAbsoluteJump(src - 13, (DWORD64)dst);
  WriteShortJump(src, -13);

  VirtualProtect((LPVOID)(src - 13), 15, oldProtection, &oldProtection);

  return gate;
}

bool EraseWithNop(void* start, int length) {
  DWORD oldProtection;
  if (!VirtualProtect(start, length, PAGE_EXECUTE_READWRITE, &oldProtection)) {
    return false;
  }
  memset(start, 0x90, length);
  if (!VirtualProtect(start, length, oldProtection, &oldProtection)) {
    return false;
  }
  return true;
}

void NopIfHackActive(BYTE* start, int hookLength, int nopLength) {
  // Makes a detour to some hand-written assembly that checks if the hack is enabled.
  // If hack is enabled, the result is equivalent to hopping over a few instructions.
  // If hack is disabled, it is just as if there was no detour.
  int nStolenBytes = max(0, hookLength - nopLength);
  BYTE* detourCode = new BYTE[39 + nStolenBytes];
  memcpy_s(detourCode, 24,
    "\x49\xbc\x00\x11\x00\x11\x00\x11\x00\x11\x45\x8a\x1c\x24\x66\x9c\x41\x80\xfb\x00\x74\x0f\x66\x9d",
    24);
  if (nStolenBytes > 0) {
    detourCode[21] += nStolenBytes;
    memcpy_s(detourCode + 24, nStolenBytes, start + nopLength, nStolenBytes);
  }
  memcpy_s(detourCode + 24 + nStolenBytes, 15,
    "\x49\xbb\x00\x11\x00\x11\x00\x11\x00\x11\x41\xff\xe3\x66\x9d",
    15);
  *(DWORD64*)(detourCode + 2) = (DWORD64)&hackActive;
  *(DWORD64*)(detourCode + 26 + nStolenBytes) = (DWORD64)start + nopLength + nStolenBytes;
  Detour(start, hookLength, detourCode, 39 + nStolenBytes);
  delete[] detourCode;
}

HANDLE hConsole;
void UpdateConsole() {
  // home for the cursor
  COORD coordScreen = { 0, 0 };
  DWORD cCharsWritten;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  DWORD dwConSize;
  if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    return;
  dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
  if (!FillConsoleOutputCharacter(hConsole, (WCHAR)' ', dwConSize, coordScreen, &cCharsWritten))
    return;
  if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    return;
  if (!FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten))
    return;
  SetConsoleCursorPosition(hConsole, coordScreen);

  // display info
  std::cout << "Press [DEL] to toggle auto-respawn on/off\n\n"
    << "Closing this console will close the game too.\n"
    << "When closing the game, the game will not actually exit until the console has been closed too.\n\n"
    << "************************************\n\n" << std::endl
    << "auto-respawn is currently " << (hackActive ? "enabled" : "disabled") << std::endl;
}

// Custom injected code entry point
void Run()
{
  // Initialize thread data - DO NOT REMOVE
  il2cpp_thread_attach(il2cpp_domain_get());

  // If you would like to output to a new console window, use il2cppi_new_console() to open one and redirect stdout
  il2cppi_new_console();
  hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

  BYTE* baseAddress = (BYTE*)il2cppi_get_base_address();

  // do not destroy shards when the objective is unlocked.
  NopIfHackActive(baseAddress + 0x80dd52, 13, 10);

  // remove call to StopAndReturnGlowParticles (the call yields exceptions when done several times)
  EraseWithNop(baseAddress + 0x80e060, 8);

  // remove check whether triangle has triangleHintArrow
  EraseWithNop(baseAddress + 0x6bbbc1, 82);

  // install function hooks
  ShardSpawner_Spawn_Trampoline = (spawnFunction)Trampoline((BYTE*)ShardSpawner_Spawn, (BYTE*)ShardSpawner_Spawn_Modified, 16);
  Triangle_OnCollect_Trampoline = (onCollectFunction)TriangleOnCollectTrampoline((BYTE*)Triangle_OnCollect, (BYTE*)Triangle_OnCollect_Modified);
  PersistentManager_OnSceneLoaded_Trampoline = (onSceneLoadedFunction)Trampoline((BYTE*)PersistentManager_OnSceneLoaded, (BYTE*)PersistentManager_OnSceneLoaded_Modified, 14);

  // replace call to YngletPlayer_get_isInsideBubble by our own function
  EraseWithNop(baseAddress + 0x80eebc, 13); // erase test and jump after the call, will replace manually
  BYTE* detourLoc = Detour(baseAddress + 0x80eebc, 13,
    (BYTE*)"\x49\x89\xf8\x48\xb8\x11\x00\x11\x00\x11\x00\x11\x00\xff\xd0\x84\xc0\x74\x0d\x49\xbb\x11\x00\x11\x00\x11\x00\x11\x00\x41\xff\xe3\x49\xbb\x11\x00\x11\x00\x11\x00\x11\x00\x41\xff\xe3",
    45);
  *(DWORD64*)(detourLoc + 5) = (DWORD64)ShouldShardRespawn;
  *(DWORD64*)(detourLoc + 21) = (DWORD64)baseAddress + 0x80eec9;
  *(DWORD64*)(detourLoc + 34) = (DWORD64)baseAddress + 0x80f15b;

  UpdateConsole();
  while (true) {
    if (GetAsyncKeyState(VK_DELETE)) {
      hackActive = !hackActive;
      UpdateConsole();
      Sleep(200);
    }
    Sleep(30);
  }
}