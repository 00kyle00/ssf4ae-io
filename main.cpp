#include <Windows.h>
#include <Xinput.h>
#include "SharedMemory.h"

namespace
{
  HMODULE xinput;
  SharedMemory mem("ssf4ae-overlay-communication-pipe", 16);

  unsigned SimplifyInput(XINPUT_STATE* input) {
    const int triggerDead = 50;
    const int thumbDead = 14300;

    unsigned code = 0;

    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_X)              ? 0x01 : 0; //LP
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_Y)              ? 0x02 : 0; //MP
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 0x04 : 0; //HP
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  ? 0x07 : 0; //PPP

    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_A)              ? 0x08 : 0; //LK
    code |= (input->Gamepad.wButtons & XINPUT_GAMEPAD_B)              ? 0x10 : 0; //MK
    code |= (input->Gamepad.bRightTrigger > triggerDead)              ? 0x20 : 0; //HK
    code |= (input->Gamepad.bLeftTrigger > triggerDead)               ? 0x38 : 0; //KKK

    code |= (input->Gamepad.sThumbLX > thumbDead)                     ? 0x040 : 0; // >
    code |= (input->Gamepad.sThumbLX < -thumbDead)                    ? 0x080 : 0; // <
    code |= (input->Gamepad.sThumbLY < -thumbDead)                    ? 0x100 : 0; // ^
    code |= (input->Gamepad.sThumbLY > thumbDead)                     ? 0x200 : 0; // \/

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
    mem.ptr<unsigned>()[dwUserIndex] = SimplifyInput(pState);
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
