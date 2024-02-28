#include "pch.h"
#include "Gamepad.hpp"

#include <SetupAPI.h>
#include <cstdio>
#include <cstdlib>
#include <stdlib.h>

#pragma comment (lib, "Setupapi.lib")
#pragma comment (lib, "Hid.lib")

Gamepad::Gamepad() {
  HDEVINFO hDevInfoSet;
  SP_DEVINFO_DATA devInfoData;
  SP_DEVICE_INTERFACE_DATA devIfcData;
  PSP_DEVICE_INTERFACE_DETAIL_DATA devIfcDetailData;

  DWORD dwMemberIdx = 0, dwSize, dwType;
  GUID hidGuid;
  PBYTE byteArrayBuffer;

  HidD_GetHidGuid(&hidGuid);
  /* printf("HID GUID: {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n", */
  /*   hidGuid.Data1, hidGuid.Data2, hidGuid.Data3, */
  /*   hidGuid.Data4[0], hidGuid.Data4[1], hidGuid.Data4[2], hidGuid.Data4[3], */
  /*   hidGuid.Data4[4], hidGuid.Data4[5], hidGuid.Data4[6], hidGuid.Data4[7]); */

  hDevInfoSet = SetupDiGetClassDevs(&hidGuid, NULL, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

  devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

  if (hDevInfoSet != INVALID_HANDLE_VALUE) {
    while (SetupDiEnumDeviceInfo(hDevInfoSet, ++dwMemberIdx, &devInfoData)) {
      SetupDiGetDeviceRegistryPropertyA(hDevInfoSet, &devInfoData, SPDRP_HARDWAREID, &dwType, NULL, 0, &dwSize);

      byteArrayBuffer = (PBYTE)malloc(dwSize * sizeof(BYTE));

      if (SetupDiGetDeviceRegistryPropertyA(hDevInfoSet, &devInfoData, SPDRP_HARDWAREID, &dwType, byteArrayBuffer, dwSize, NULL)) {
        constexpr char* vid_sony       = (char*)"VID_054C";
        constexpr char* pid_usb_ds4    = (char*)"PID_05C4";
        constexpr char* pid_usb_ds4_v2 = (char*)"PID_09CC";
        constexpr char* pid_dualsense  = (char*)"PID_0Df2";

        const char* babStr = (char*)byteArrayBuffer;
        bool isDualsense = strstr(babStr, pid_dualsense);

        // If match the substings of the vendor id and the product ids
        if (strstr(babStr, vid_sony) && (strstr(babStr, pid_usb_ds4) || strstr(babStr, pid_usb_ds4_v2) || isDualsense)) {
          devIfcData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
          SetupDiEnumDeviceInterfaces(hDevInfoSet, NULL, &hidGuid, dwMemberIdx, &devIfcData);

          SetupDiGetDeviceInterfaceDetail(hDevInfoSet, &devIfcData, NULL, 0, &dwSize, NULL);

          devIfcDetailData = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc(dwSize);
          devIfcDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

          SetupDiGetDeviceInterfaceDetail(hDevInfoSet, &devIfcData, devIfcDetailData, dwSize, &dwSize, NULL);

          hHidDeviceObject = CreateFile((devIfcDetailData->DevicePath), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);

          // Getting device capabilities
          HidD_GetPreparsedData(hHidDeviceObject, &preparsedData);
          HidP_GetCaps(preparsedData, &caps);

          free(devIfcDetailData);
          free(byteArrayBuffer);

          gamepadAllocated = true;
          touchpadButtonOffset = isDualsense ? 10 : 7;
          break;
        }
      }
      free(byteArrayBuffer);
    }
  }

  SetupDiDestroyDeviceInfoList(hDevInfoSet);
}

Gamepad::~Gamepad() {
  if (gamepadAllocated) {
    HidD_FreePreparsedData(preparsedData);
    CloseHandle(hHidDeviceObject);
  }
}

uint8_t Gamepad::handleTouchpad() {
  if (gamepadAllocated) {
    DWORD dwRead;
    PBYTE inputReport = (PBYTE)malloc(caps.InputReportByteLength);

    ReadFile(hHidDeviceObject, inputReport, caps.InputReportByteLength, &dwRead, 0);

    // If the touchpad button is pressed
    if (inputReport[touchpadButtonOffset] & 0b0000'0010) {
      constexpr uint16_t centerX = 1919 / 2;

      uint16_t touchX = ((uint16_t)inputReport[37] << 8 | (uint16_t)inputReport[36]) & 0b0000'1111'1111'1111;

      if (touchX > centerX)
        return TOUCHPAD_RIGHT_SIDE;
      else
        return TOUCHPAD_LEFT_SIDE;
    }

    free(inputReport);
  }

  return TOUCHPAD_NOTHING;
}

void Gamepad::printInfo(const std::shared_ptr<CVarManagerWrapper>& _globalCvarManager) const {
  _globalCvarManager->log("inputLenght: " + std::to_string(caps.InputReportByteLength));
  _globalCvarManager->log("gamepadAllocated: " + std::to_string(gamepadAllocated));
  _globalCvarManager->log("touchpad offset: " + std::to_string(touchpadButtonOffset));
}

