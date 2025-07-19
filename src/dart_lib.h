#pragma once
#include "dart_dll.h"

// Initialize Dart
bool DartLib_Initialize(const DartDllConfig& config);
// Load a script, with an optional package configuration location. The package
// configuration is usually in ".dart_tool/package_config.json".
Dart_Isolate DartLib_LoadScript(const char* script_uri,
                                                const char* package_config,
                                                void* isolate_data = nullptr);
// Gets the pointer provided by the `isolate_data` parameter in DartDll_LoadScript after
// calling Dart_CurrentIsolateData
void* DartLib_GetUserIsolateData(void* isolate_group_data);
// Run "main" from the supplied library, usually one you got from
// Dart_RootLibrary()
Dart_Handle DartLib_RunMain(Dart_Handle library);
// Drain the microtask queue. This is necessary if you're using any async code
// or Futures, and using Dart_Invoke over DartDll_RunMain or Dart_RunLoop.
// Otherwise you're not giving the main isolate the opportunity to drain the task queue
// and complete pending Futures.
Dart_Handle DartLib_DrainMicrotaskQueue();
// Shutdown Dart
bool DartLib_Shutdown();

