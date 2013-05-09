#pragma once

#include <Windows.h>

class SharedMemory {
public:
  SharedMemory(const char* name, int size) {
    hfile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, name);
    addr = MapViewOfFile(hfile, FILE_MAP_ALL_ACCESS, 0, 0, size);
  }
  ~SharedMemory() {
    UnmapViewOfFile(addr);
    CloseHandle(hfile);
  }
  void* ptr() const { return addr; }
  template<class T>
  T* ptr() const { return (T*) addr; }
private:
  HANDLE hfile;
  void* addr;
};
