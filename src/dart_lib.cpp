#include "dart_dll.h"

bool DartLib_Initialize(const DartDllConfig& config) {
  return DartDll_Initialize(config);
}

Dart_Isolate DartLib_LoadScript(const char* script_uri,
                                const char* package_config,
                                void* isolate_data) {
  return DartDll_LoadScript(script_uri, package_config, isolate_data);
}

void* DartLib_GetUserIsolateData(void* isolate_group_data) {
  return DartDll_GetUserIsolateData(isolate_group_data);
}

Dart_Handle DartLib_RunMain(Dart_Handle library) {
  return DartDll_RunMain(library);
}

Dart_Handle DartLib_DrainMicrotaskQueue() {
  return DartDll_DrainMicrotaskQueue();
}

bool DartLib_Shutdown() {
  return DartDll_Shutdown();
}

