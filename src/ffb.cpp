#include "plugin.h"
#include "lua_module.h"
#include "pch.h"
#include <InitGuid.h>
using namespace plugin;
//#define STRICT
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
//#pragma warning(pop)

#include <math.h>
//#include "resource.h"
#include <iostream>
#include <string>

//-----------------------------------------------------------------------------
// Function prototypes 
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumFFDevicesCallback(const DIDEVICEINSTANCE* pInst, VOID* pContext);
BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext);
VOID FreeDirectInput();
HRESULT SetDeviceForcesXY();

//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=nullptr; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=nullptr; } }

DIJOYSTATE idk = {};
LPDIRECTINPUT8          g_pDI = nullptr;
LPDIRECTINPUTDEVICE8    g_pDevice = nullptr;
LPDIRECTINPUTEFFECT     g_pEffect = nullptr;
BOOL                    g_bActive = TRUE;
DWORD                   g_dwNumForceFeedbackAxis = 0;
INT                     g_nXForce = 0;
INT                     g_nYForce = 0;
HWND					HWNDProgram = nullptr;





//-----------------------------------------------------------------------------
// Name: InitDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
std::string formatError(HRESULT hr) {
	switch (hr) {
	case DI_OK:                     return "DI_OK";
	case DIERR_INVALIDPARAM:        return "DIERR_INVALIDPARAM";
	case DIERR_NOTINITIALIZED:      return "DIERR_NOTINITIALIZED";
	case DIERR_ALREADYINITIALIZED:  return "DIERR_ALREADYINITIALIZED";
	case DIERR_INPUTLOST:           return "DIERR_INPUTLOST";
	case DIERR_ACQUIRED:            return "DIERR_ACQUIRED";
	case DIERR_NOTACQUIRED:         return "DIERR_NOTACQUIRED";
	case E_HANDLE:                  return "E_HANDLE";
	case DIERR_DEVICEFULL:          return "DIERR_DEVICEFULL";
	case DIERR_DEVICENOTREG:        return "DIERR_DEVICENOTREG";
	default:                        return "UNKNOWN";
	}
}



HRESULT InitDirectInput()
{
	DIPROPDWORD dipdw;
	HRESULT hr;

	//g_pDevice->Unacquire();

	// Register with the DirectInput subsystem and get a pointer
	// to a IDirectInput interface we can use.
	printf("if (FAILED(hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION,\n");
	if (FAILED(hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION,
		IID_IDirectInput8, (VOID**)&g_pDI, nullptr)))
	{
		return hr;
	}

	// Look for a force feedback device we can use
	printf("if (FAILED(hr = g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL,\n");
	if (FAILED(hr = g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL,
		EnumFFDevicesCallback, nullptr,
		DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK)))
	{
		return hr;
	}
	printf("if (!g_pDevice)\n");
	if (!g_pDevice)
	{
		printf("[C++] No DI Device !g_pDevice Line 112\n");
		return 420;
	}

	// Set the data format to "simple joystick" - a predefined data format. A
	// data format specifies which controls on a device we are interested in,
	// and how they should be reported.
	//
	// This tells DirectInput that we will be passing a DIJOYSTATE structure to
	// IDirectInputDevice8::GetDeviceState(). Even though we won't actually do
	// it in this sample. But setting the data format is important so that the
	// DIJOFS_* values work properly.
	printf("if (FAILED(hr = g_pDevice->SetDataFormat(&c_dfDIJoystick)))\n");
	if (FAILED(hr = g_pDevice->SetDataFormat(&c_dfDIJoystick)))
		return hr;

	// Set the cooperative level to let DInput know how this device should
	// interact with the system and with other DInput applications.
	// Exclusive access is required in order to perform force feedback.
	printf("if (FAILED(hr = g_pDevice->SetCooperativeLevel(HWNDProgram, DISCL_EXCLUSIVE | DISCL_FOREGROUND)))\n");
	if (FAILED(hr = g_pDevice->SetCooperativeLevel(HWNDProgram, DISCL_EXCLUSIVE | DISCL_FOREGROUND)))
	{
		return hr;
	}

	// Since we will be playing force feedback effects, we should disable the
	// auto-centering spring.
	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = FALSE;
	printf("if (FAILED(hr = g_pDevice->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph)))\n");
	if (FAILED(hr = g_pDevice->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph)))
		return hr;
	printf("if (FAILED(hr = g_pDevice->EnumObjects(EnumAxesCallback,\n");
	// Enumerate and count the axes of the joystick 
	if (FAILED(hr = g_pDevice->EnumObjects(EnumAxesCallback,
		(VOID*)&g_dwNumForceFeedbackAxis, DIDFT_AXIS)))
		return hr;

	// This simple sample only supports one or two axis joysticks
	g_dwNumForceFeedbackAxis = 1;


	//if (g_dwNumForceFeedbackAxis > 2)
	//	g_dwNumForceFeedbackAxis = 2;

	// This application needs only one effect: Applying raw forces.
	DWORD rgdwAxes[2] = { DIJOFS_X, DIJOFS_Y };
	LONG rglDirection[2] = { 0, 0 };
	DICONSTANTFORCE cf = { 0 };

	DIEFFECT eff;
	ZeroMemory(&eff, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
	eff.dwDuration = INFINITE;
	eff.dwSamplePeriod = 0;
	eff.dwGain = DI_FFNOMINALMAX;
	eff.dwTriggerButton = DIEB_NOTRIGGER;
	eff.dwTriggerRepeatInterval = 0;
	eff.cAxes = g_dwNumForceFeedbackAxis;
	eff.rgdwAxes = rgdwAxes;
	eff.rglDirection = rglDirection;
	eff.lpEnvelope = 0;
	eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
	eff.lpvTypeSpecificParams = &cf;
	eff.dwStartDelay = 0;
	printf("if (FAILED(hr = g_pDevice->CreateEffect(GUID_ConstantForce,\n");
	// Create the prepared effect
	if (FAILED(hr = g_pDevice->CreateEffect(GUID_ConstantForce,
		&eff, &g_pEffect, nullptr)))
	{
		return hr;
	}

	if (!g_pEffect)
		return E_FAIL;

	return S_OK;
}

//
//BOOL GetData()
//{
//	//g_pDevice->GetDeviceDataQQQQ
//	DIDEVICEOBJECTDATA rgdod[100];
//
//	DIDEVICEOBJECTDATA *lpdidod;
//		DWORD dwItems = 10;
//	DWORD hres = g_pDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),	rgdod,&dwItems,	0);
//	g_pDevice->GetDeviceState(sizeof(DIJOYSTATE))
//	
//	if (SUCCEEDED(hres)) {
//		printf("dwItems = %d\n", dwItems);
//		for (int i = 0; i < 50; i++)
//		{
//			lpdidod = &rgdod[i];
//			printf("i=%d, lpdidod->dwOfs = %d, lpdidod->dwData = %d\n",i, lpdidod->dwOfs, lpdidod->dwData);
//		}
//
//		// Buffer successfully flushed. 
//		// dwItems = Number of elements flushed. 
//		if (hres == DI_BUFFEROVERFLOW) {
//			// Buffer had overflowed. 
//		}
//		else
//		{
//			//printf("hres = %s\n", formatError(hres).c_str());
//		}
//	}
//	return false;
//}

//-----------------------------------------------------------------------------
// Name: EnumAxesCallback()
// Desc: Callback function for enumerating the axes on a joystick and counting
//       each force feedback enabled axis
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi,
	VOID* pContext)
{
	auto pdwNumForceFeedbackAxis = reinterpret_cast<DWORD*>(pContext);

	if ((pdidoi->dwFlags & DIDOI_FFACTUATOR) != 0)
		(*pdwNumForceFeedbackAxis)++;

	return DIENUM_CONTINUE;
}




//-----------------------------------------------------------------------------
// Name: EnumFFDevicesCallback()
// Desc: Called once for each enumerated force feedback device. If we find
//       one, create a device interface on it so we can play with it.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumFFDevicesCallback(const DIDEVICEINSTANCE* pInst,
	VOID* pContext)
{
	LPDIRECTINPUTDEVICE8 pDevice;
	HRESULT hr;

	// Obtain an interface to the enumerated force feedback device.

	hr = g_pDI->CreateDevice(GUID_Joystick, &pDevice, nullptr);
	//hr = g_pDI->CreateDevice(pInst->guidInstance, &pDevice, nullptr);

	//pInst->guidInstance
	// If it failed, then we can't use this device for some
	// bizarre reason.  (Maybe the user unplugged it while we
	// were in the middle of enumerating it.)  So continue enumerating
	if (FAILED(hr))
		return DIENUM_CONTINUE;

	// We successfully created an IDirectInputDevice8.  So stop looking 
	// for another one.
	g_pDevice = pDevice;
	pDevice = nullptr;

	return DIENUM_STOP;
}

//-----------------------------------------------------------------------------
// Name: FreeDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
VOID FreeDirectInput()
{
	// Unacquire the device one last time just in case 
	// the app tried to exit while the device is still acquired.
	if (g_pDevice)
		g_pDevice->Unacquire();

	// Release any DirectInput objects.
	SAFE_RELEASE(g_pEffect);
	SAFE_RELEASE(g_pDevice);
	SAFE_RELEASE(g_pDI);
}



//-----------------------------------------------------------------------------
// Name: SetDeviceForcesXY()
// Desc: Apply the X and Y forces to the effect we prepared.
//-----------------------------------------------------------------------------
HRESULT SetDeviceForcesXY()
{
	// Modifying an effect is basically the same as creating a new one, except
	// you need only specify the parameters you are modifying
	LONG rglDirection;
	DICONSTANTFORCE cf;


	cf.lMagnitude = g_nXForce;
	rglDirection = 0;

	//else
	//{
	//	// If two force feedback axis, then apply magnitude from both directions 
	//	rglDirection[0] = g_nXForce;
	//	rglDirection[1] = g_nYForce;
	//	cf.lMagnitude = (DWORD)sqrt((double)g_nXForce * (double)g_nXForce +
	//		(double)g_nYForce * (double)g_nYForce);
	//}

	//printf("[C++] g_dwNumForceFeedbackAxis %d\n", g_dwNumForceFeedbackAxis);
	//HRESULT hr;
	//if (FAILED(hr = InitDirectInput()))
	//{
	//	printf("[C++] InitDirectInput failed Errorcode: %d, Error message: %s\n", hr, formatError(hr).c_str());
	//}
	DIEFFECT eff;
	ZeroMemory(&eff, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
	eff.cAxes = g_dwNumForceFeedbackAxis;
	eff.rglDirection = &rglDirection;
	eff.lpEnvelope = 0;
	eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
	eff.lpvTypeSpecificParams = &cf;
	eff.dwStartDelay = 0;
	//std::cout << g_dwNumForceFeedbackAxis << std::endl;
	// Now set the new parameters and start the effect immediately.
	return g_pEffect->SetParameters(&eff, DIEP_DIRECTION |
		DIEP_TYPESPECIFICPARAMS |
		DIEP_START);
}



sol::table open(sol::this_state ts)
{
	sol::state_view lua(ts);
	sol::table module = lua.create_table();
	module["VERSION"] = "1.1";
	module.set_function("Init", [](sol::this_state ts) {
		sol::state_view lua{ ts };
		HWNDProgram = GetForegroundWindow();

		lua["print"]("Initializing ");
		lua["print"](HWNDProgram);

		HRESULT hr;
		if (FAILED(hr = InitDirectInput()))
		{
			printf("[C++] InitDirectInput failed Errorcode: %d, Error message: %s\n", hr, formatError(hr).c_str());
		}
		//lua["print"]("[C++] InitDirectInput failed Errorcode: %d, Error message: %s\n", hr, formatError(hr).c_str());

		});
	module.set_function("SetFFB", [](sol::this_state ts, int Val) {
		sol::state_view lua{ ts };

		g_nXForce = (int)(Val);
		HRESULT hr;
		if (FAILED(hr = SetDeviceForcesXY()))
		{
			//lua["print"]("[C++] SetDeviceForcesXY failed Errorcode : %d, Error message : %s\n", hr, formatError(hr).c_str());
			printf("[C++] SetDeviceForcesXY failed Errorcode: %d, Error message: %s\n", hr, formatError(hr).c_str());
		}

		});
	module.set_function("FreeDirectInput", [](sol::this_state ts) {
		sol::state_view lua{ ts };
		FreeDirectInput();
		});
	module.set_function("GetData", [](sol::this_state ts) {
		sol::state_view lua{ ts };
		DWORD hres = g_pDevice->GetDeviceState(sizeof(DIJOYSTATE), &idk);
		sol::table result = lua.create_table();

		if (SUCCEEDED(hres)) {



			result[0] = idk.lRx;
			result[1] = idk.lRy;
			result[2] = idk.lRz;
			result[3] = idk.lX;
			result[4] = idk.lY;
			result[5] = idk.lZ;
			result[6] = idk.rglSlider[0];
			result[7] = idk.rglSlider[1];

			return result;

		}
		return result;
		});
	module.set_function("AcquireDevice", [](sol::this_state ts) {
		sol::state_view lua{ ts };
		HRESULT hr;
		if (FAILED(hr = g_pDevice->Acquire()))
		{
			printf("[C++] g_pDevice->Acquire failed Errorcode: %d, Error message: %s\n", hr, formatError(hr).c_str());
		}
		//lua["print"]("[C++] g_pDevice->Acquire failed Errorcode: %d, Error message: %s\n", hr, formatError(hr).c_str());
		});

	module.set_function("debug", [](sol::this_state ts) {
		sol::state_view lua{ ts };
		printf("%p\n", GetForegroundWindow());
		});
	module.set_function("SpawnConsole", [](sol::this_state ts) {
		sol::state_view lua{ ts };

		if (!AllocConsole()) {
			// Add some error handling here.
			// You can call GetLastError() to get more info about the error.
			return;
		}

		// std::cout, std::clog, std::cerr, std::cin
		FILE* fDummy;
		freopen_s(&fDummy, "CONOUT$", "w", stdout);
		freopen_s(&fDummy, "CONOUT$", "w", stderr);
		freopen_s(&fDummy, "CONIN$", "r", stdin);
		std::cout.clear();
		std::clog.clear();
		std::cerr.clear();
		std::cin.clear();

		// std::wcout, std::wclog, std::wcerr, std::wcin
		HANDLE hConOut = CreateFile(("CONOUT$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		HANDLE hConIn = CreateFile(("CONIN$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
		SetStdHandle(STD_ERROR_HANDLE, hConOut);
		SetStdHandle(STD_INPUT_HANDLE, hConIn);
		std::wcout.clear();
		std::wclog.clear();
		std::wcerr.clear();
		std::wcin.clear();
		});
	module.set_function("Print", [](sol::this_state ts, std::string penis) {
		sol::state_view lua{ ts };
		printf("[SA LUA] %s\n", penis.c_str());
		});
	return module;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
		// Uncomment these lines to make module unloadable
		//HMODULE hModule;
		//GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&DllMain), &hModule);
	}
	return TRUE;
}

SOL_MODULE_ENTRYPOINT(open);
