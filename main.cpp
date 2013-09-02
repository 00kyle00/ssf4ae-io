#include <Windows.h>
#include <Xinput.h>
#include "SharedMemory.h"
#include <fstream>

namespace
{
  HMODULE xinput;
  SharedMemory mem("ssf4ae-overlay-communication-pipe", 16);

  unsigned buttonsmap[] = {
    0x001, 0x002, 0x004, 0x007,
    0x008, 0x010, 0x020, 0x038
  };
  int triggerDead = 50;
  int thumbDead = 14300;

  void ReadConfig() {
    static bool read = false;
    if(read) return;
    read = true;

    std::ifstream cfg("input.cfg");
    if(cfg.is_open()) {
      for(int i=0; i<8; ++i)
        cfg >> buttonsmap[i];
      cfg >> triggerDead;
      cfg >> thumbDead;
    }
  }

  unsigned SimplifyInput(XINPUT_STATE* input) {
    ReadConfig();

    unsigned code = 0;
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_X)              ? buttonsmap[0] : 0; //LP
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_Y)              ? buttonsmap[1] : 0; //MP
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? buttonsmap[2] : 0; //HP
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  ? buttonsmap[3] : 0; //PPP

    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_A)              ? buttonsmap[4] : 0; //LK
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_B)              ? buttonsmap[5] : 0; //MK
    code |= (input->Gamepad.bRightTrigger > triggerDead)              ? buttonsmap[6] : 0; //HK
    code |= (input->Gamepad.bLeftTrigger > triggerDead)               ? buttonsmap[7] : 0; //KKK

    code |= (input->Gamepad.sThumbLX > thumbDead)                     ? 0x040 : 0; // >
    code |= (input->Gamepad.sThumbLX < -thumbDead)                    ? 0x080 : 0; // <
    code |= (input->Gamepad.sThumbLY < -thumbDead)                    ? 0x100 : 0; // ^
    code |= (input->Gamepad.sThumbLY > thumbDead)                     ? 0x200 : 0; // \/

    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)     ? 0x040 : 0;
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)      ? 0x080 : 0;
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)        ? 0x100 : 0;
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)      ? 0x200 : 0;

    return code;
  }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
  if(fdwReason == DLL_PROCESS_ATTACH) {
    xinput = LoadLibrary(L"C:\\Windows\\System32\\xinput1_3.dll");
  }

  return TRUE;
}

template<class T>
T set_call(T& ptr, const char* name) {
  if(ptr == 0)
    ptr = (T) GetProcAddress(xinput, name);
  return ptr;
}

template<class T>
T set_call(T& ptr, int ord) {
  if(ptr == 0)
    ptr = (T) GetProcAddress(xinput, (const char*) ord);
  return ptr;
}

void WINAPI XInputEnable(BOOL state)
{
  static void (WINAPI *orig)(BOOL state);
  return set_call(orig, __FUNCTION__)(state);
}

DWORD WINAPI XInputGetBatteryInformation(DWORD dwUserIndex, BYTE devType, XINPUT_BATTERY_INFORMATION *pBatteryInformation)
{
  static DWORD (WINAPI *orig)(DWORD dwUserIndex, BYTE devType, XINPUT_BATTERY_INFORMATION *pBatteryInformation);
  return set_call(orig, __FUNCTION__)(dwUserIndex, devType, pBatteryInformation);
}

DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES *pCapabilities)
{
  static DWORD (WINAPI *orig)(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES *pCapabilities);
  return set_call(orig, __FUNCTION__)(dwUserIndex, dwFlags, pCapabilities);
}

DWORD WINAPI XInputGetDSoundAudioDeviceGuids(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid)
{
  static DWORD (WINAPI *orig)(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid);
  return set_call(orig, __FUNCTION__)(dwUserIndex, pDSoundRenderGuid, pDSoundCaptureGuid);
}

DWORD WINAPI XInputGetKeystroke(DWORD dwUserIndex, DWORD dwReserved, PXINPUT_KEYSTROKE pKeystroke)
{
  static DWORD (WINAPI *orig)(DWORD dwUserIndex, DWORD dwReserved, PXINPUT_KEYSTROKE pKeystroke);
  return set_call(orig, __FUNCTION__)(dwUserIndex, dwReserved, pKeystroke);
}

DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE *pState)
{
  static DWORD (WINAPI *orig)(DWORD dwUserIndex, XINPUT_STATE *pState);
  DWORD ret = set_call(orig, __FUNCTION__)(dwUserIndex, pState);
  if(ret == ERROR_SUCCESS && dwUserIndex < 4) {
    mem.ptr<unsigned>()[dwUserIndex] = SimplifyInput(pState);
  }
  return ret;
}

DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
{
  static DWORD (WINAPI *orig)(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);
  return set_call(orig, __FUNCTION__)(dwUserIndex, pVibration);
}

DWORD WINAPI XInputOrd100(DWORD dwUserIndex, XINPUT_STATE *pState)
{
  static DWORD (WINAPI *orig)(DWORD dwUserIndex, XINPUT_STATE *pState);
  DWORD ret = set_call(orig, 100)(dwUserIndex, pState);
  if(ret == ERROR_SUCCESS && dwUserIndex < 4) {
    mem.ptr<unsigned>()[2*dwUserIndex] = SimplifyInput(pState);
    mem.ptr<unsigned>()[2*dwUserIndex+1] = timeGetTime();
  }
  return ret;
}

DWORD WINAPI XInputOrd101(DWORD dwUserIndex, DWORD unk, void* ptr)
{
  static DWORD (WINAPI *orig)(DWORD dwUserIndex, DWORD unk, void* ptr);
  return set_call(orig, 101)(dwUserIndex, unk, ptr);
}

DWORD WINAPI XInputOrd102(DWORD dwUserIndex)
{
  static DWORD (WINAPI *orig)(DWORD dwUserIndex);
  return set_call(orig, 102)(dwUserIndex);
}

DWORD WINAPI XInputOrd103(DWORD dwUserIndex)
{
  static DWORD (WINAPI *orig)(DWORD dwUserIndex);
  return set_call(orig, 103)(dwUserIndex);
}
