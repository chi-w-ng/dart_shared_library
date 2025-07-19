#include "isolate_setup.h"
#include <iostream>
#include <include/dart_api.h>
#include <include/dart_embedder_api.h>
#include <bin/builtin.h>
#include <bin/dartutils.h>
#include <bin/dfe.h>
#include <bin/isolate_data.h>
#include <bin/loader.h>
#include <bin/snapshot_utils.h>
#include <bin/vmservice_impl.h>
#include <platform/utils.h>
#include <fstream>
#include <vector>
#include <filesystem>
#include "isolate_snapshot_data.inc"  // Large file here
#include "vm_snapshot_data.inc"

const uint8_t* EmbeddedSnapshot::embedded_isolate_snapshot_binary_data =
    isolate_snapshot_binary_data;
const uint8_t* EmbeddedSnapshot::embedded_vm_snapshot_binary_data =
    vm_snapshot_binary_data;

using namespace dart::bin;
namespace fs = std::filesystem;

static fs::path get_folder_from_path(const fs::path& script_uri) {
  return script_uri.parent_path();  // Extracts directory
}

// reads isolate_snapshot_data.bin file for core isolate snapshot data
static std::vector<uint8_t> loadIsolateSnapshotData(const std::string& filepath) {
  // 1. Open file at end to immediately get size
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file) {
    return {};
  }
  // 2. Get size and pre-allocate vector (critical for performance)
  const auto size = file.tellg();
  file.seekg(0);
  std::vector<uint8_t> buffer(size);
  // 3. Single bulk read (fastest method)
  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    return {};
  }
  return buffer;
}

namespace {
class DllIsolateGroupData : public IsolateGroupData {
 public:
  DllIsolateGroupData(const char* url,
                      const char* packages_file,
                      AppSnapshot* app_snapshot,
                      bool isolate_run_app_snapshot,
                      void* callback_data = nullptr)
      : IsolateGroupData(url,
                         nullptr,  // asset_resolution_base
                         packages_file,
                         app_snapshot,
                         isolate_run_app_snapshot),
        callback_data(callback_data) {}

  void* callback_data;
};
}  // namespace

Dart_Handle SetupCoreLibraries(Dart_Isolate isolate,
                               IsolateData* isolate_data,
                               bool is_isolate_group_start,                               
                               const char** resolved_packages_config) {
  auto isolate_group_data = isolate_data->isolate_group_data();
  const auto packages_file = isolate_data->packages_file();
  const auto script_uri = isolate_group_data->script_url;
  Dart_Handle result;

  // Prepare builtin and other core libraries for use to resolve URIs.
  // Set up various closures, e.g: printing, timers etc.
  // Set up package configuration for URI resolution.
  result = DartUtils::PrepareForScriptLoading(false, true, true);
  if (Dart_IsError(result)) return result;

  // Setup packages config if specified.  
  result = DartUtils::SetupPackageConfig(packages_file);
  if (Dart_IsError(result)) return result;

  if (!Dart_IsNull(result) && resolved_packages_config != nullptr) {
    result = Dart_StringToCString(result, resolved_packages_config);
    if (Dart_IsError(result)) return result;

    if (is_isolate_group_start) {
      IsolateGroupData* isolate_group_data = isolate_data->isolate_group_data();
      isolate_group_data->set_resolved_packages_config(
          *resolved_packages_config);
    }
  }

  result = Dart_SetEnvironmentCallback(DartUtils::EnvironmentCallback);
  if (Dart_IsError(result)) return result;

  // Setup the native resolver as the snapshot does not carry it.
  Builtin::SetNativeResolver(Builtin::kBuiltinLibrary);
  Builtin::SetNativeResolver(Builtin::kIOLibrary);
  Builtin::SetNativeResolver(Builtin::kCLILibrary);
  VmService::SetNativeResolver();

  result = DartUtils::SetupIOLibrary(nullptr, script_uri, true);
  if (Dart_IsError(result)) return result;

  return result;
}

Dart_Isolate CreateKernelIsolate(const char* script_uri,
                                 const char* main,
                                 const char* package_root,
                                 const char* package_config,
                                 Dart_IsolateFlags* flags,
                                 void* callback_data,
                                 char** error) {
  const char* kernel_snapshot_uri = dfe.frontend_filename();
  const char* uri =
      kernel_snapshot_uri != NULL ? kernel_snapshot_uri : script_uri;

  const uint8_t* kernel_service_buffer = nullptr;
  intptr_t kernel_service_buffer_size = 0;
  dfe.LoadKernelService(&kernel_service_buffer, &kernel_service_buffer_size);

  DllIsolateGroupData* isolate_group_data =
      new DllIsolateGroupData(uri, package_config, nullptr, false);
  isolate_group_data->SetKernelBufferUnowned(
      const_cast<uint8_t*>(kernel_service_buffer), kernel_service_buffer_size);
  IsolateData* isolate_data = new IsolateData(isolate_group_data);

  dart::embedder::IsolateCreationData data = {script_uri, main, flags,
                                              isolate_group_data, isolate_data};

  Dart_Isolate isolate = dart::embedder::CreateKernelServiceIsolate(
      data, kernel_service_buffer, kernel_service_buffer_size, error);
  if (isolate == nullptr) {
    std::cerr << "Error creating kernel isolate: " << *error << std::endl;
    delete isolate_data;
    delete isolate_group_data;
    return isolate;
  }

  // Needed?
  Dart_EnterIsolate(isolate);
  Dart_EnterScope();

  Dart_SetLibraryTagHandler(Loader::LibraryTagHandler);
  SetupCoreLibraries(isolate, isolate_data, false, nullptr);

  Dart_ExitScope();
  Dart_ExitIsolate();

  return isolate;
}

Dart_Isolate CreateVmServiceIsolate(const char* script_uri,
                                    const char* main,
                                    const char* package_root,
                                    const char* package_config,
                                    Dart_IsolateFlags* flags,
                                    void* callback_data,
                                    int service_port,
                                    char** error) {
  DllIsolateGroupData* isolate_group_data = new DllIsolateGroupData(
      script_uri, package_config, nullptr, false, callback_data);
  IsolateData* isolate_data = new IsolateData(isolate_group_data);

  flags->load_vmservice_library = true;
  const uint8_t* isolate_snapshot_data = nullptr;
  const uint8_t* isolate_snapshot_instructions = nullptr;

  dart::embedder::IsolateCreationData data = {script_uri, main, flags,
                                              isolate_group_data, isolate_data};

  dart::embedder::VmServiceConfiguration vm_config = {
      "127.0.0.1", service_port, nullptr, true, true, true};

  Dart_Isolate isolate = dart::embedder::CreateVmServiceIsolate(
      data, vm_config, isolate_snapshot_data, isolate_snapshot_instructions,
      error);
  if (isolate == nullptr) {
    std::cerr << "Error creating VM Service Isolate: " << *error << std::endl;
    delete isolate_data;
    delete isolate_group_data;
    return nullptr;
  }

  Dart_EnterIsolate(isolate);
  Dart_EnterScope();
  Dart_SetEnvironmentCallback(DartUtils::EnvironmentCallback);
  Dart_ExitScope();
  Dart_ExitIsolate();

  return isolate;
}

Dart_Isolate CreateIsolate(bool is_main_isolate,
                           const char* script_uri,
                           const char* name,
                           const char* package_config,
                           Dart_IsolateFlags* flags,
                           void* callback_data,
                           char** error) {
  Dart_Handle result;
  uint8_t* kernel_buffer = nullptr;
  intptr_t kernel_buffer_size = 0;
  AppSnapshot* app_snapshot = nullptr;
  
  bool isolate_run_app_snapshot = false;
  const uint8_t* isolate_snapshot_data = EmbeddedSnapshot::embedded_isolate_snapshot_binary_data;
  const uint8_t* isolate_snapshot_instructions = NullByte::get();

  if (!is_main_isolate) {
    app_snapshot = Snapshot::TryReadAppSnapshot(script_uri);
    if (app_snapshot != nullptr && app_snapshot->IsJITorAOT()) {
      if (app_snapshot->IsAOT()) {
        *error = dart::Utils::SCreate(
            "The uri(%s) provided to `Isolate.spawnUri()` is an "
            "AOT snapshot and the JIT VM cannot spawn an isolate using it.",
            script_uri);
        delete app_snapshot;
        return nullptr;
      }
      isolate_run_app_snapshot = true;
      const uint8_t* ignore_vm_snapshot_data;
      const uint8_t* ignore_vm_snapshot_instructions;
      app_snapshot->SetBuffers(
          &ignore_vm_snapshot_data, &ignore_vm_snapshot_instructions,
          &isolate_snapshot_data, &isolate_snapshot_instructions);
    }
  }
  
  if (kernel_buffer == nullptr && !isolate_run_app_snapshot) {
    int exit_code = 0;
    dfe.ReadScript(script_uri, 
                    app_snapshot, 
                    &kernel_buffer,
                   &kernel_buffer_size, 
                    true);
  }
  if (kernel_buffer == nullptr &&
      kernel_buffer_size < 0) {
    kernel_buffer_size = 0;
  }
  flags->null_safety = true;

  DllIsolateGroupData* isolate_group_data = new DllIsolateGroupData(
      script_uri, package_config, nullptr, false, callback_data);
  isolate_group_data->SetKernelBufferNewlyOwned(kernel_buffer,
                                                kernel_buffer_size);
  const uint8_t* platform_kernel_buffer = nullptr;
  intptr_t platform_kernel_buffer_size = 0;
  dfe.LoadPlatform(&platform_kernel_buffer, &platform_kernel_buffer_size);
  if (platform_kernel_buffer == nullptr) {
    platform_kernel_buffer = kernel_buffer;
    platform_kernel_buffer_size = kernel_buffer_size;
  }
  Dart_Isolate isolate = nullptr;
  IsolateData* isolate_data = new IsolateData(isolate_group_data);
  if (isolate_snapshot_data && isolate_snapshot_instructions) {
    isolate = Dart_CreateIsolateGroup(script_uri, name, isolate_snapshot_data,
                                      isolate_snapshot_instructions, flags,
                                      isolate_group_data, isolate_data, error);
  } else {
    isolate = Dart_CreateIsolateGroupFromKernel(
        script_uri, name, platform_kernel_buffer, platform_kernel_buffer_size,
        flags, isolate_group_data, isolate_data, error);
   }
  if (isolate == nullptr) {
    std::cerr << "Error creating isolate " << name << ": " << *error
              << std::endl;

    delete isolate_data;
    delete isolate_group_data;
    return nullptr;
  }

  std::cout << "Created isolate " << name << std::endl;
  Dart_EnterScope();

  // TODO - Set Library Tag Handler, SetupCoreLibraries
  result = Dart_SetLibraryTagHandler(Loader::LibraryTagHandler);
  if (Dart_IsError(result)) {
    std::cerr << "Error setting LibraryTag Handler: " << *error << std::endl;
    Dart_ExitScope();
    Dart_ShutdownIsolate();
    return nullptr;
  }

  const char* resolved_packages_config = nullptr;
  result = SetupCoreLibraries(isolate, isolate_data, true,
                              &resolved_packages_config);
  if (Dart_IsError(result)) {
    std::cerr << "Error setting up core libraries for isolate: " << *error
              << std::endl;
    Dart_ExitScope();
    Dart_ShutdownIsolate();
    return nullptr;
  }

  if (kernel_buffer == nullptr && !Dart_IsKernelIsolate(isolate)) {
    uint8_t* application_kernel_buffer = nullptr;
    intptr_t application_kernel_buffer_size = 0;
    int exit_code = 0;
    dfe.CompileAndReadScript(script_uri, &application_kernel_buffer,
                             &application_kernel_buffer_size, error, &exit_code,
                             resolved_packages_config, true, false);
    if (application_kernel_buffer == nullptr) {
      std::cerr << "Error reading Dart script " << script_uri << ": " << *error
                << std::endl;
      Dart_ExitScope();
      Dart_ShutdownIsolate();
      return nullptr;
    }

    isolate_group_data->SetKernelBufferNewlyOwned(
        application_kernel_buffer, application_kernel_buffer_size);
    kernel_buffer = application_kernel_buffer;
    kernel_buffer_size = application_kernel_buffer_size;
  }

  if (kernel_buffer != nullptr) {
    Dart_Handle result =
        Dart_LoadScriptFromKernel(kernel_buffer, kernel_buffer_size);
    if (Dart_IsError(result)) {
      std::cerr << "Error loading script: " << Dart_GetError(result)
                << std::endl;
      Dart_ExitScope();
      Dart_ShutdownIsolate();
      return nullptr;
    }
  }

  Dart_ExitScope();
  Dart_ExitIsolate();
  *error = Dart_IsolateMakeRunnable(isolate);

  if (*error != nullptr) {
    std::cerr << "Error making isolate runnable: " << error << std::endl;
    Dart_EnterIsolate(isolate);
    Dart_ShutdownIsolate();
    return nullptr;
  }

  return isolate;
}

void* GetUserIsolateData(void* isolate_data) {
  if (isolate_data == nullptr) {
    return nullptr;
  }
  IsolateGroupData* isolate_group_data =
      reinterpret_cast<IsolateData*>(isolate_data)->isolate_group_data();

  if (isolate_group_data == nullptr) {
    return nullptr;
  }
  return static_cast<DllIsolateGroupData*>(isolate_group_data)->callback_data;
}

void DeleteIsolateData(void* raw_isolate_group_data, void* raw_isolate_data) {
  auto isolate_data = reinterpret_cast<IsolateData*>(raw_isolate_data);
  delete isolate_data;
}

void DeleteIsolateGroupData(void* raw_isolate_group_data) {
  auto isolate_group_data =
      reinterpret_cast<DllIsolateGroupData*>(raw_isolate_group_data);
  delete isolate_group_data;
}
