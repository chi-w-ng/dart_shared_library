#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>  // For uint8_t, uint32_t
#include <windows.h>  // For GetModuleFileNameA
#include <filesystem>

namespace fs = std::filesystem;

static fs::path GetExeDirectory() {
  char buffer[MAX_PATH];
  GetModuleFileNameA(nullptr, buffer, MAX_PATH);  // Get full .exe path
  return fs::path(buffer).parent_path();          // Extract directory
}

int main() {
  std::vector<std::string> files{"vm_snapshot_data.bin",
                                 "isolate_snapshot_data.bin"};
  for (auto& iFileName : files) {
    fs::path fileName{iFileName};
    auto folder = GetExeDirectory();
    auto inFile = folder / iFileName;
    fs::path outputFile = inFile;
    outputFile.replace_extension(".txt");
    std::ofstream outputStrem(outputFile, std::ios::trunc);
    // 1. Open the file in binary mode
    std::ifstream file(inFile, std::ios::binary | std::ios::ate);
    if (!outputStrem.good() || !file.is_open()) {
      std::cerr << "Error: Could not open file " << inFile << std::endl;
      return 1;
    }

    // 2. Get file size and read contents
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
      std::cerr << "Error: Failed to read file contents" << std::endl;
      return 1;
    }

    // 3. Process the buffer (like your example)
    std::cout << "File contents (" << size << " bytes):" << std::endl;
    outputStrem << "const std::array<uint8_t," << std::to_string(size)
                << "> = snapshotData = {";
    for (uint32_t i = 0; i < size; ++i) {
      uint8_t value = buffer[i];
      outputStrem << std::to_string(value);
      if (i < size - 1) {
        outputStrem << ",";
      }
    }
    outputStrem << "};" << std::endl;
  }
   return 0;
}