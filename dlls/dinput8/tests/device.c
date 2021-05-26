/*
 * Copyright (c) 2011 Lucas Fialho Zawacki
 * Copyright (c) 2006 Vitaliy Margolen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define DIRECTINPUT_VERSION 0x0800

#define COBJMACROS
#include <windows.h>

#include "wine/test.h"
#include "windef.h"
#include "dinput.h"

struct enum_data {
    IDirectInput8A *pDI;
    DIACTIONFORMATA *lpdiaf;
    IDirectInputDevice8A *keyboard;
    IDirectInputDevice8A *mouse;
    const char* username;
    int ndevices;
};

/* Dummy GUID */
static const GUID ACTION_MAPPING_GUID = { 0x1, 0x2, 0x3, { 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb } };

enum {
    DITEST_AXIS,
    DITEST_BUTTON,
    DITEST_KEYBOARDSPACE,
    DITEST_MOUSEBUTTON0,
    DITEST_YAXIS
};

static DIACTIONA actionMapping[]=
{
  /* axis */
  { 0, 0x01008A01 /* DIAXIS_DRIVINGR_STEER */,      0, { "Steer.\0" }   },
  /* button */
  { 1, 0x01000C01 /* DIBUTTON_DRIVINGR_SHIFTUP */,  0, { "Upshift.\0" } },
  /* keyboard key */
  { 2, DIKEYBOARD_SPACE,                            0, { "Missile.\0" } },
  /* mouse button */
  { 3, DIMOUSE_BUTTON0,                             0, { "Select\0" }   },
  /* mouse axis */
  { 4, DIMOUSE_YAXIS,                               0, { "Y Axis\0" }   }
};
/* By placing the memory pointed to by lptszActionName right before memory with PAGE_NOACCESS
 * one can find out that the regular ansi string termination is not respected by EnumDevicesBySemantics.
 * Adding a double termination, making it a valid wide string termination, made the test succeed.
 * Therefore it looks like ansi version of EnumDevicesBySemantics forwards the string to
 * the wide variant without conversation. */

static void flush_events(void)
{
    int diff = 200;
    int min_timeout = 100;
    DWORD time = GetTickCount() + diff;

    while (diff > 0)
    {
        if (MsgWaitForMultipleObjects(0, NULL, FALSE, min_timeout, QS_ALLINPUT) == WAIT_TIMEOUT)
            break;
        diff = time - GetTickCount();
        min_timeout = 50;
    }
}

static void test_device_input(IDirectInputDevice8A *lpdid, DWORD event_type, DWORD event, UINT_PTR expected)
{
    HRESULT hr;
    DIDEVICEOBJECTDATA obj_data;
    DWORD data_size = 1;
    int i;

    hr = IDirectInputDevice8_Acquire(lpdid);
    ok (SUCCEEDED(hr), "Failed to acquire device hr=%08x\n", hr);

    if (event_type == INPUT_KEYBOARD)
        keybd_event(0, event, KEYEVENTF_SCANCODE, 0);

    if (event_type == INPUT_MOUSE)
        mouse_event( event, 0, 0, 0, 0);

    flush_events();
    IDirectInputDevice8_Poll(lpdid);
    hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);

    if (data_size != 1)
    {
        win_skip("We're not able to inject input into Windows dinput8 with events\n");
        IDirectInputDevice_Unacquire(lpdid);
        return;
    }

    ok (obj_data.uAppData == expected, "Retrieval of action failed uAppData=%lu expected=%lu\n", obj_data.uAppData, expected);

    /* Check for buffer overflow */
    for (i = 0; i < 17; i++)
        if (event_type == INPUT_KEYBOARD)
        {
            keybd_event( VK_SPACE, DIK_SPACE, 0, 0);
            keybd_event( VK_SPACE, DIK_SPACE, KEYEVENTF_KEYUP, 0);
        }
        else if (event_type == INPUT_MOUSE)
        {
            mouse_event(MOUSEEVENTF_LEFTDOWN, 1, 1, 0, 0);
            mouse_event(MOUSEEVENTF_LEFTUP, 1, 1, 0, 0);
        }

    flush_events();
    IDirectInputDevice8_Poll(lpdid);

    data_size = 1;
    hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);
    ok(hr == DI_BUFFEROVERFLOW, "GetDeviceData() failed: %08x\n", hr);
    data_size = 1;
    hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);
    ok(hr == DI_OK && data_size == 1, "GetDeviceData() failed: %08x cnt:%d\n", hr, data_size);

    /* drain device's queue */
    while (data_size == 1)
    {
        hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);
        ok(hr == DI_OK, "GetDeviceData() failed: %08x cnt:%d\n", hr, data_size);
        if (hr != DI_OK) break;
    }

    IDirectInputDevice_Unacquire(lpdid);
}

static void test_build_action_map(IDirectInputDevice8A *lpdid, DIACTIONFORMATA *lpdiaf,
                                  int action_index, DWORD expected_type, DWORD expected_inst)
{
    HRESULT hr;
    DIACTIONA *actions;
    DWORD instance, type, how;
    GUID assigned_to;
    DIDEVICEINSTANCEA ddi;

    ddi.dwSize = sizeof(ddi);
    IDirectInputDevice_GetDeviceInfo(lpdid, &ddi);

    hr = IDirectInputDevice8_BuildActionMap(lpdid, lpdiaf, NULL, DIDBAM_HWDEFAULTS);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    actions = lpdiaf->rgoAction;
    instance = DIDFT_GETINSTANCE(actions[action_index].dwObjID);
    type = DIDFT_GETTYPE(actions[action_index].dwObjID);
    how = actions[action_index].dwHow;
    assigned_to = actions[action_index].guidInstance;

    ok (how == DIAH_USERCONFIG || how == DIAH_DEFAULT, "Action was not set dwHow=%08x\n", how);
    ok (instance == expected_inst, "Action not mapped correctly instance=%08x expected=%08x\n", instance, expected_inst);
    ok (type == expected_type, "Action type not mapped correctly type=%08x expected=%08x\n", type, expected_type);
    ok (IsEqualGUID(&assigned_to, &ddi.guidInstance), "Action and device GUID do not match action=%d\n", action_index);
}

static BOOL CALLBACK enumeration_callback(const DIDEVICEINSTANCEA *lpddi, IDirectInputDevice8A *lpdid,
                                          DWORD dwFlags, DWORD dwRemaining, LPVOID pvRef)
{
    HRESULT hr;
    DIPROPDWORD dp;
    DIPROPRANGE dpr;
    DIPROPSTRING dps;
    WCHAR usernameW[MAX_PATH];
    DWORD username_size = MAX_PATH;
    struct enum_data *data = pvRef;
    DWORD cnt;
    DIDEVICEOBJECTDATA buffer[5];
    IDirectInputDevice8A *lpdid2;

    if (!data) return DIENUM_CONTINUE;

    data->ndevices++;

    /* Convert username to WCHAR */
    if (data->username != NULL)
    {
        username_size = MultiByteToWideChar(CP_ACP, 0, data->username, -1, usernameW, 0);
        MultiByteToWideChar(CP_ACP, 0, data->username, -1, usernameW, username_size);
    }
    else
        GetUserNameW(usernameW, &username_size);

    /* collect the mouse and keyboard */
    if (IsEqualGUID(&lpddi->guidInstance, &GUID_SysKeyboard))
    {
        IDirectInputDevice_AddRef(lpdid);
        data->keyboard = lpdid;

        ok (dwFlags & DIEDBS_MAPPEDPRI1, "Keyboard should be mapped as pri1 dwFlags=%08x\n", dwFlags);
    }

    if (IsEqualGUID(&lpddi->guidInstance, &GUID_SysMouse))
    {
        IDirectInputDevice_AddRef(lpdid);
        data->mouse = lpdid;

        ok (dwFlags & DIEDBS_MAPPEDPRI1, "Mouse should be mapped as pri1 dwFlags=%08x\n", dwFlags);
    }

    /* Creating second device object to check if it has the same username */
    hr = IDirectInput_CreateDevice(data->pDI, &lpddi->guidInstance, &lpdid2, NULL);
    ok(SUCCEEDED(hr), "IDirectInput_CreateDevice() failed: %08x\n", hr);

    /* Building and setting an action map */
    /* It should not use any pre-stored mappings so we use DIDBAM_HWDEFAULTS */
    hr = IDirectInputDevice8_BuildActionMap(lpdid, data->lpdiaf, NULL, DIDBAM_HWDEFAULTS);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    /* Device has no data format and thus can't be acquired */
    hr = IDirectInputDevice8_Acquire(lpdid);
    ok (hr == DIERR_INVALIDPARAM, "Device was acquired before SetActionMap hr=%08x\n", hr);

    hr = IDirectInputDevice8_SetActionMap(lpdid, data->lpdiaf, data->username, 0);
    ok (SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

    /* Some joysticks may have no suitable actions and thus should not be tested */
    if (hr == DI_NOEFFECT) return DIENUM_CONTINUE;

    /* Test username after SetActionMap */
    dps.diph.dwSize = sizeof(dps);
    dps.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dps.diph.dwObj = 0;
    dps.diph.dwHow  = DIPH_DEVICE;
    dps.wsz[0] = '\0';

    hr = IDirectInputDevice_GetProperty(lpdid, DIPROP_USERNAME, &dps.diph);
    ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
    ok (!lstrcmpW(usernameW, dps.wsz), "Username not set correctly expected=%s, got=%s\n", wine_dbgstr_w(usernameW), wine_dbgstr_w(dps.wsz));

    dps.wsz[0] = '\0';
    hr = IDirectInputDevice_GetProperty(lpdid2, DIPROP_USERNAME, &dps.diph);
    ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
    ok (!lstrcmpW(usernameW, dps.wsz), "Username not set correctly expected=%s, got=%s\n", wine_dbgstr_w(usernameW), wine_dbgstr_w(dps.wsz));

    /* Test buffer size */
    memset(&dp, 0, sizeof(dp));
    dp.diph.dwSize = sizeof(dp);
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwHow  = DIPH_DEVICE;

    hr = IDirectInputDevice_GetProperty(lpdid, DIPROP_BUFFERSIZE, &dp.diph);
    ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
    ok (dp.dwData == data->lpdiaf->dwBufferSize, "SetActionMap must set the buffer, buffersize=%d\n", dp.dwData);

    cnt = 1;
    hr = IDirectInputDevice_GetDeviceData(lpdid, sizeof(buffer[0]), buffer, &cnt, 0);
    ok(hr == DIERR_NOTACQUIRED, "GetDeviceData() failed hr=%08x\n", hr);

    /* Test axis range */
    memset(&dpr, 0, sizeof(dpr));
    dpr.diph.dwSize = sizeof(dpr);
    dpr.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dpr.diph.dwHow  = DIPH_DEVICE;

    hr = IDirectInputDevice_GetProperty(lpdid, DIPROP_RANGE, &dpr.diph);
    /* Only test if device supports the range property */
    if (SUCCEEDED(hr))
    {
        ok (dpr.lMin == data->lpdiaf->lAxisMin, "SetActionMap must set the min axis range expected=%d got=%d\n", data->lpdiaf->lAxisMin, dpr.lMin);
        ok (dpr.lMax == data->lpdiaf->lAxisMax, "SetActionMap must set the max axis range expected=%d got=%d\n", data->lpdiaf->lAxisMax, dpr.lMax);
    }

    /* SetActionMap has set the data format so now it should work */
    hr = IDirectInputDevice8_Acquire(lpdid);
    ok (SUCCEEDED(hr), "Acquire failed hr=%08x\n", hr);

    cnt = 1;
    hr = IDirectInputDevice_GetDeviceData(lpdid, sizeof(buffer[0]), buffer, &cnt, 0);
    ok(hr == DI_OK, "GetDeviceData() failed hr=%08x\n", hr);

    /* SetActionMap should not work on an acquired device */
    hr = IDirectInputDevice8_SetActionMap(lpdid, data->lpdiaf, NULL, 0);
    ok (hr == DIERR_ACQUIRED, "SetActionMap succeeded with an acquired device hr=%08x\n", hr);

    IDirectInputDevice_Release(lpdid2);

    return DIENUM_CONTINUE;
}

static void test_appdata_property_vs_map(struct enum_data *data)
{
    HRESULT hr;
    DIPROPPOINTER dp;

    dp.diph.dwSize = sizeof(dp);
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwHow = DIPH_BYID;
    dp.diph.dwObj = DIDFT_MAKEINSTANCE(DIK_SPACE) | DIDFT_PSHBUTTON;
    dp.uData = 10;
    hr = IDirectInputDevice8_SetProperty(data->keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    test_device_input(data->keyboard, INPUT_KEYBOARD, DIK_SPACE, 10);

    dp.diph.dwHow = DIPH_BYID;
    dp.diph.dwObj = DIDFT_MAKEINSTANCE(DIK_V) | DIDFT_PSHBUTTON;
    dp.uData = 11;
    hr = IDirectInputDevice8_SetProperty(data->keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(hr == DIERR_OBJECTNOTFOUND, "IDirectInputDevice8_SetProperty should not find key that's not in the action map hr=%08x\n", hr);

    /* setting format should reset action map */
    hr = IDirectInputDevice8_SetDataFormat(data->keyboard, &c_dfDIKeyboard);
    ok(SUCCEEDED(hr), "SetDataFormat failed: %08x\n", hr);

    test_device_input(data->keyboard, INPUT_KEYBOARD, DIK_SPACE, -1);

    dp.diph.dwHow = DIPH_BYID;
    dp.diph.dwObj = DIDFT_MAKEINSTANCE(DIK_V) | DIDFT_PSHBUTTON;
    dp.uData = 11;
    hr = IDirectInputDevice8_SetProperty(data->keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    test_device_input(data->keyboard, INPUT_KEYBOARD, DIK_V, 11);

    /* back to action map */
    hr = IDirectInputDevice8_SetActionMap(data->keyboard, data->lpdiaf, NULL, 0);
    ok(SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

    test_device_input(data->keyboard, INPUT_KEYBOARD, DIK_SPACE, 2);
}

static void test_action_mapping(void)
{
    HRESULT hr;
    HINSTANCE hinst = GetModuleHandleA(NULL);
    IDirectInput8A *pDI = NULL;
    DIACTIONFORMATA af;
    DIPROPSTRING dps;
    struct enum_data data =  {pDI, &af, NULL, NULL, NULL, 0};
    HWND hwnd;

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER, &IID_IDirectInput8A, (LPVOID*)&pDI);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("ActionMapping requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8 Create failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hr = IDirectInput8_Initialize(pDI,hinst, DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("ActionMapping requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8 Initialize failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    memset (&af, 0, sizeof(af));
    af.dwSize = sizeof(af);
    af.dwActionSize = sizeof(DIACTIONA);
    af.dwDataSize = 4 * ARRAY_SIZE(actionMapping);
    af.dwNumActions = ARRAY_SIZE(actionMapping);
    af.rgoAction = actionMapping;
    af.guidActionMap = ACTION_MAPPING_GUID;
    af.dwGenre = 0x01000000; /* DIVIRTUAL_DRIVING_RACE */
    af.dwBufferSize = 32;

    /* This enumeration builds and sets the action map for all devices */
    data.pDI = pDI;
    hr = IDirectInput8_EnumDevicesBySemantics(pDI, 0, &af, enumeration_callback, &data, DIEDBSFL_ATTACHEDONLY);
    ok (SUCCEEDED(hr), "EnumDevicesBySemantics failed: hr=%08x\n", hr);

    if (data.keyboard)
        IDirectInputDevice_Release(data.keyboard);

    if (data.mouse)
        IDirectInputDevice_Release(data.mouse);

    /* Repeat tests with a non NULL user */
    data.username = "Ninja Brian";
    hr = IDirectInput8_EnumDevicesBySemantics(pDI, NULL, &af, enumeration_callback, &data, DIEDBSFL_ATTACHEDONLY);
    ok (SUCCEEDED(hr), "EnumDevicesBySemantics failed: hr=%08x\n", hr);

    hwnd = CreateWindowExA(WS_EX_TOPMOST, "static", "dinput",
            WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    ok(hwnd != NULL, "failed to create window\n");
    SetCursorPos(50, 50);

    if (data.keyboard != NULL)
    {
        /* Test keyboard BuildActionMap */
        test_build_action_map(data.keyboard, data.lpdiaf, DITEST_KEYBOARDSPACE, DIDFT_PSHBUTTON, DIK_SPACE);
        /* Test keyboard input */
        test_device_input(data.keyboard, INPUT_KEYBOARD, DIK_SPACE, 2);

        /* setting format should reset action map */
        hr = IDirectInputDevice8_SetDataFormat(data.keyboard, &c_dfDIKeyboard);
        ok (SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);

        test_device_input(data.keyboard, INPUT_KEYBOARD, DIK_SPACE, -1);

        /* back to action map */
        hr = IDirectInputDevice8_SetActionMap(data.keyboard, data.lpdiaf, NULL, 0);
        ok (SUCCEEDED(hr), "SetActionMap should succeed hr=%08x\n", hr);

        test_device_input(data.keyboard, INPUT_KEYBOARD, DIK_SPACE, 2);

        test_appdata_property_vs_map(&data);

        /* Test BuildActionMap with no suitable actions for a device */
        IDirectInputDevice_Unacquire(data.keyboard);
        af.dwDataSize = 4 * DITEST_KEYBOARDSPACE;
        af.dwNumActions = DITEST_KEYBOARDSPACE;

        hr = IDirectInputDevice8_BuildActionMap(data.keyboard, data.lpdiaf, NULL, DIDBAM_HWDEFAULTS);
        ok (hr == DI_NOEFFECT, "BuildActionMap should have no effect with no actions hr=%08x\n", hr);

        hr = IDirectInputDevice8_SetActionMap(data.keyboard, data.lpdiaf, NULL, 0);
        ok (hr == DI_NOEFFECT, "SetActionMap should have no effect with no actions to map hr=%08x\n", hr);

        af.dwDataSize = 4 * ARRAY_SIZE(actionMapping);
        af.dwNumActions = ARRAY_SIZE(actionMapping);

        /* test DIDSAM_NOUSER */
        dps.diph.dwSize = sizeof(dps);
        dps.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dps.diph.dwObj = 0;
        dps.diph.dwHow = DIPH_DEVICE;
        dps.wsz[0] = '\0';

        hr = IDirectInputDevice_GetProperty(data.keyboard, DIPROP_USERNAME, &dps.diph);
        ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
        ok (dps.wsz[0] != 0, "Expected any username, got=%s\n", wine_dbgstr_w(dps.wsz));

        hr = IDirectInputDevice8_SetActionMap(data.keyboard, data.lpdiaf, NULL, DIDSAM_NOUSER);
        ok (SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

        dps.diph.dwSize = sizeof(dps);
        dps.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dps.diph.dwObj = 0;
        dps.diph.dwHow = DIPH_DEVICE;
        dps.wsz[0] = '\0';

        hr = IDirectInputDevice_GetProperty(data.keyboard, DIPROP_USERNAME, &dps.diph);
        ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
        ok (dps.wsz[0] == 0, "Expected empty username, got=%s\n", wine_dbgstr_w(dps.wsz));

        IDirectInputDevice_Release(data.keyboard);
    }

    if (data.mouse != NULL)
    {
        /* Test mouse BuildActionMap */
        test_build_action_map(data.mouse, data.lpdiaf, DITEST_MOUSEBUTTON0, DIDFT_PSHBUTTON, 0x03);
        test_build_action_map(data.mouse, data.lpdiaf, DITEST_YAXIS, DIDFT_RELAXIS, 0x01);

        test_device_input(data.mouse, INPUT_MOUSE, MOUSEEVENTF_LEFTDOWN, 3);

        IDirectInputDevice_Release(data.mouse);
    }

    DestroyWindow(hwnd);
    IDirectInput_Release(pDI);
}

static void test_save_settings(void)
{
    HRESULT hr;
    HINSTANCE hinst = GetModuleHandleA(NULL);
    IDirectInput8A *pDI = NULL;
    DIACTIONFORMATA af;
    IDirectInputDevice8A *pKey;

    static const GUID mapping_guid = { 0xcafecafe, 0x2, 0x3, { 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb } };
    static const GUID other_guid = { 0xcafe, 0xcafe, 0x3, { 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb } };

    static DIACTIONA actions[] = {
        { 0, DIKEYBOARD_A , 0, { "Blam" } },
        { 1, DIKEYBOARD_B , 0, { "Kapow"} }
    };
    static const DWORD results[] = {
        DIDFT_MAKEINSTANCE(DIK_A) | DIDFT_PSHBUTTON,
        DIDFT_MAKEINSTANCE(DIK_B) | DIDFT_PSHBUTTON
    };
    static const DWORD other_results[] = {
        DIDFT_MAKEINSTANCE(DIK_C) | DIDFT_PSHBUTTON,
        DIDFT_MAKEINSTANCE(DIK_D) | DIDFT_PSHBUTTON
    };

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER, &IID_IDirectInput8A, (LPVOID*)&pDI);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("ActionMapping requires dinput8\n");
        return;
    }
    ok (SUCCEEDED(hr), "DirectInput8 Create failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hr = IDirectInput8_Initialize(pDI,hinst, DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("ActionMapping requires dinput8\n");
        return;
    }
    ok (SUCCEEDED(hr), "DirectInput8 Initialize failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hr = IDirectInput_CreateDevice(pDI, &GUID_SysKeyboard, &pKey, NULL);
    ok (SUCCEEDED(hr), "IDirectInput_Create device failed hr: 0x%08x\n", hr);
    if (FAILED(hr)) return;

    memset (&af, 0, sizeof(af));
    af.dwSize = sizeof(af);
    af.dwActionSize = sizeof(DIACTIONA);
    af.dwDataSize = 4 * ARRAY_SIZE(actions);
    af.dwNumActions = ARRAY_SIZE(actions);
    af.rgoAction = actions;
    af.guidActionMap = mapping_guid;
    af.dwGenre = 0x01000000; /* DIVIRTUAL_DRIVING_RACE */
    af.dwBufferSize = 32;

    /* Easy case. Ask for default mapping, save, ask for previous map and read it back */
    hr = IDirectInputDevice8_BuildActionMap(pKey, &af, NULL, DIDBAM_HWDEFAULTS);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);
    ok (results[0] == af.rgoAction[0].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[0], af.rgoAction[0].dwObjID);

    ok (results[1] == af.rgoAction[1].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[1], af.rgoAction[1].dwObjID);

    hr = IDirectInputDevice8_SetActionMap(pKey, &af, NULL, DIDSAM_FORCESAVE);
    ok (SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

    if (hr == DI_SETTINGSNOTSAVED)
    {
        skip ("Can't test saving settings if SetActionMap returns DI_SETTINGSNOTSAVED\n");
        return;
    }

    af.rgoAction[0].dwObjID = 0;
    af.rgoAction[1].dwObjID = 0;
    memset(&af.rgoAction[0].guidInstance, 0, sizeof(GUID));
    memset(&af.rgoAction[1].guidInstance, 0, sizeof(GUID));

    hr = IDirectInputDevice8_BuildActionMap(pKey, &af, NULL, 0);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    ok (results[0] == af.rgoAction[0].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[0], af.rgoAction[0].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[0].guidInstance), "Action should be mapped to keyboard\n");

    ok (results[1] == af.rgoAction[1].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[1], af.rgoAction[1].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[1].guidInstance), "Action should be mapped to keyboard\n");

    /* Test that a different action map with no pre-stored settings, in spite of the flags,
       does not try to load mappings and instead applies the default mapping */
    af.guidActionMap = other_guid;

    af.rgoAction[0].dwObjID = 0;
    af.rgoAction[1].dwObjID = 0;
    memset(&af.rgoAction[0].guidInstance, 0, sizeof(GUID));
    memset(&af.rgoAction[1].guidInstance, 0, sizeof(GUID));

    hr = IDirectInputDevice8_BuildActionMap(pKey, &af, NULL, 0);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    ok (results[0] == af.rgoAction[0].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[0], af.rgoAction[0].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[0].guidInstance), "Action should be mapped to keyboard\n");

    ok (results[1] == af.rgoAction[1].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[1], af.rgoAction[1].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[1].guidInstance), "Action should be mapped to keyboard\n");

    af.guidActionMap = mapping_guid;
    /* Hard case. Customized mapping, save, ask for previous map and read it back */
    af.rgoAction[0].dwObjID = other_results[0];
    af.rgoAction[0].dwHow = DIAH_USERCONFIG;
    af.rgoAction[0].guidInstance = GUID_SysKeyboard;
    af.rgoAction[1].dwObjID = other_results[1];
    af.rgoAction[1].dwHow = DIAH_USERCONFIG;
    af.rgoAction[1].guidInstance = GUID_SysKeyboard;

    hr = IDirectInputDevice8_SetActionMap(pKey, &af, NULL, DIDSAM_FORCESAVE);
    ok (SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

    if (hr == DI_SETTINGSNOTSAVED)
    {
        skip ("Can't test saving settings if SetActionMap returns DI_SETTINGSNOTSAVED\n");
        return;
    }

    af.rgoAction[0].dwObjID = 0;
    af.rgoAction[1].dwObjID = 0;
    memset(&af.rgoAction[0].guidInstance, 0, sizeof(GUID));
    memset(&af.rgoAction[1].guidInstance, 0, sizeof(GUID));

    hr = IDirectInputDevice8_BuildActionMap(pKey, &af, NULL, 0);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    ok (other_results[0] == af.rgoAction[0].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", other_results[0], af.rgoAction[0].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[0].guidInstance), "Action should be mapped to keyboard\n");

    ok (other_results[1] == af.rgoAction[1].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", other_results[1], af.rgoAction[1].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[1].guidInstance), "Action should be mapped to keyboard\n");

    IDirectInputDevice_Release(pKey);
    IDirectInput_Release(pDI);
}

static void test_mouse_keyboard(void)
{
    HRESULT hr;
    HWND hwnd, di_hwnd = INVALID_HANDLE_VALUE;
    IDirectInput8A *di = NULL;
    IDirectInputDevice8A *di_mouse, *di_keyboard;
    UINT raw_devices_count;
    RAWINPUTDEVICE raw_devices[3];

    hwnd = CreateWindowExA(WS_EX_TOPMOST, "static", "dinput", WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    ok(hwnd != NULL, "CreateWindowExA failed\n");

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER, &IID_IDirectInput8A, (LPVOID*)&di);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("test_mouse_keyboard requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8Create failed: %08x\n", hr);

    hr = IDirectInput8_Initialize(di, GetModuleHandleA(NULL), DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("test_mouse_keyboard requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "IDirectInput8_Initialize failed: %08x\n", hr);

    hr = IDirectInput8_CreateDevice(di, &GUID_SysMouse, &di_mouse, NULL);
    ok(SUCCEEDED(hr), "IDirectInput8_CreateDevice failed: %08x\n", hr);
    hr = IDirectInputDevice8_SetDataFormat(di_mouse, &c_dfDIMouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);

    hr = IDirectInput8_CreateDevice(di, &GUID_SysKeyboard, &di_keyboard, NULL);
    ok(SUCCEEDED(hr), "IDirectInput8_CreateDevice failed: %08x\n", hr);
    hr = IDirectInputDevice8_SetDataFormat(di_keyboard, &c_dfDIKeyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);

    raw_devices_count = ARRAY_SIZE(raw_devices);
    GetRegisteredRawInputDevices(NULL, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(raw_devices_count == 0, "Unexpected raw devices registered: %d\n", raw_devices_count);

    hr = IDirectInputDevice8_Acquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    memset(raw_devices, 0, sizeof(raw_devices));
    hr = GetRegisteredRawInputDevices(raw_devices, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    todo_wine
    ok(hr == 1, "GetRegisteredRawInputDevices returned %d, raw_devices_count: %d\n", hr, raw_devices_count);
    todo_wine
    ok(raw_devices[0].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[0].usUsagePage);
    todo_wine
    ok(raw_devices[0].usUsage == 6, "Unexpected raw device usage: %x\n", raw_devices[0].usUsage);
    todo_wine
    ok(raw_devices[0].dwFlags == RIDEV_INPUTSINK, "Unexpected raw device flags: %x\n", raw_devices[0].dwFlags);
    todo_wine
    ok(raw_devices[0].hwndTarget != NULL, "Unexpected raw device target: %p\n", raw_devices[0].hwndTarget);
    hr = IDirectInputDevice8_Unacquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    GetRegisteredRawInputDevices(NULL, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(raw_devices_count == 0, "Unexpected raw devices registered: %d\n", raw_devices_count);

    if (raw_devices[0].hwndTarget != NULL)
    {
        WCHAR str[16];
        int i;

        di_hwnd = raw_devices[0].hwndTarget;
        i = GetClassNameW(di_hwnd, str, ARRAY_SIZE(str));
        ok(i == lstrlenW(L"DIEmWin"), "GetClassName returned incorrect length\n");
        ok(!lstrcmpW(L"DIEmWin", str), "GetClassName returned incorrect name for this window's class\n");

        i = GetWindowTextW(di_hwnd, str, ARRAY_SIZE(str));
        ok(i == lstrlenW(L"DIEmWin"), "GetClassName returned incorrect length\n");
        ok(!lstrcmpW(L"DIEmWin", str), "GetClassName returned incorrect name for this window's class\n");
    }

    hr = IDirectInputDevice8_Acquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    memset(raw_devices, 0, sizeof(raw_devices));
    hr = GetRegisteredRawInputDevices(raw_devices, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(hr == 1, "GetRegisteredRawInputDevices returned %d, raw_devices_count: %d\n", hr, raw_devices_count);
    ok(raw_devices[0].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[0].usUsagePage);
    ok(raw_devices[0].usUsage == 2, "Unexpected raw device usage: %x\n", raw_devices[0].usUsage);
    ok(raw_devices[0].dwFlags == RIDEV_INPUTSINK, "Unexpected raw device flags: %x\n", raw_devices[0].dwFlags);
    todo_wine
    ok(raw_devices[0].hwndTarget == di_hwnd, "Unexpected raw device target: %p\n", raw_devices[0].hwndTarget);
    hr = IDirectInputDevice8_Unacquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    GetRegisteredRawInputDevices(NULL, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(raw_devices_count == 0, "Unexpected raw devices registered: %d\n", raw_devices_count);

    if (raw_devices[0].hwndTarget != NULL)
        di_hwnd = raw_devices[0].hwndTarget;

    /* expect dinput8 to take over any activated raw input devices */
    raw_devices[0].usUsagePage = 0x01;
    raw_devices[0].usUsage = 0x05;
    raw_devices[0].dwFlags = 0;
    raw_devices[0].hwndTarget = hwnd;
    raw_devices[1].usUsagePage = 0x01;
    raw_devices[1].usUsage = 0x06;
    raw_devices[1].dwFlags = 0;
    raw_devices[1].hwndTarget = hwnd;
    raw_devices[2].usUsagePage = 0x01;
    raw_devices[2].usUsage = 0x02;
    raw_devices[2].dwFlags = 0;
    raw_devices[2].hwndTarget = hwnd;
    raw_devices_count = ARRAY_SIZE(raw_devices);
    hr = RegisterRawInputDevices(raw_devices, raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(hr == TRUE, "RegisterRawInputDevices failed\n");

    hr = IDirectInputDevice8_Acquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    hr = IDirectInputDevice8_Acquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    memset(raw_devices, 0, sizeof(raw_devices));
    hr = GetRegisteredRawInputDevices(raw_devices, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(hr == 3, "GetRegisteredRawInputDevices returned %d, raw_devices_count: %d\n", hr, raw_devices_count);
    ok(raw_devices[0].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[0].usUsagePage);
    ok(raw_devices[0].usUsage == 2, "Unexpected raw device usage: %x\n", raw_devices[0].usUsage);
    ok(raw_devices[0].dwFlags == RIDEV_INPUTSINK, "Unexpected raw device flags: %x\n", raw_devices[0].dwFlags);
    ok(raw_devices[0].hwndTarget == di_hwnd, "Unexpected raw device target: %p\n", raw_devices[0].hwndTarget);
    ok(raw_devices[1].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[1].usUsagePage);
    ok(raw_devices[1].usUsage == 5, "Unexpected raw device usage: %x\n", raw_devices[1].usUsage);
    ok(raw_devices[1].dwFlags == 0, "Unexpected raw device flags: %x\n", raw_devices[1].dwFlags);
    ok(raw_devices[1].hwndTarget == hwnd, "Unexpected raw device target: %p\n", raw_devices[1].hwndTarget);
    ok(raw_devices[2].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[1].usUsagePage);
    ok(raw_devices[2].usUsage == 6, "Unexpected raw device usage: %x\n", raw_devices[1].usUsage);
    todo_wine
    ok(raw_devices[2].dwFlags == RIDEV_INPUTSINK, "Unexpected raw device flags: %x\n", raw_devices[1].dwFlags);
    todo_wine
    ok(raw_devices[2].hwndTarget == di_hwnd, "Unexpected raw device target: %p\n", raw_devices[1].hwndTarget);
    hr = IDirectInputDevice8_Unacquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    hr = IDirectInputDevice8_Unacquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    GetRegisteredRawInputDevices(NULL, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    todo_wine
    ok(raw_devices_count == 1, "Unexpected raw devices registered: %d\n", raw_devices_count);

    IDirectInputDevice8_SetCooperativeLevel(di_mouse, hwnd, DISCL_FOREGROUND|DISCL_EXCLUSIVE);
    IDirectInputDevice8_SetCooperativeLevel(di_keyboard, hwnd, DISCL_FOREGROUND|DISCL_EXCLUSIVE);

    hr = IDirectInputDevice8_Acquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    hr = IDirectInputDevice8_Acquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    memset(raw_devices, 0, sizeof(raw_devices));
    hr = GetRegisteredRawInputDevices(raw_devices, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(hr == 3, "GetRegisteredRawInputDevices returned %d, raw_devices_count: %d\n", hr, raw_devices_count);
    ok(raw_devices[0].dwFlags == (RIDEV_CAPTUREMOUSE|RIDEV_NOLEGACY), "Unexpected raw device flags: %x\n", raw_devices[0].dwFlags);
    todo_wine
    ok(raw_devices[2].dwFlags == (RIDEV_NOHOTKEYS|RIDEV_NOLEGACY), "Unexpected raw device flags: %x\n", raw_devices[1].dwFlags);
    hr = IDirectInputDevice8_Unacquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    hr = IDirectInputDevice8_Unacquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);

    raw_devices_count = ARRAY_SIZE(raw_devices);
    hr = GetRegisteredRawInputDevices(raw_devices, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    todo_wine
    ok(hr == 1, "GetRegisteredRawInputDevices returned %d, raw_devices_count: %d\n", hr, raw_devices_count);
    ok(raw_devices[0].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[0].usUsagePage);
    ok(raw_devices[0].usUsage == 5, "Unexpected raw device usage: %x\n", raw_devices[0].usUsage);
    ok(raw_devices[0].dwFlags == 0, "Unexpected raw device flags: %x\n", raw_devices[0].dwFlags);
    ok(raw_devices[0].hwndTarget == hwnd, "Unexpected raw device target: %p\n", raw_devices[0].hwndTarget);

    IDirectInputDevice8_Release(di_mouse);
    IDirectInputDevice8_Release(di_keyboard);
    IDirectInput8_Release(di);

    DestroyWindow(hwnd);
}

static void test_keyboard_events(void)
{
    HRESULT hr;
    HWND hwnd = INVALID_HANDLE_VALUE;
    IDirectInput8A *di;
    IDirectInputDevice8A *di_keyboard;
    DIPROPDWORD dp;
    DIDEVICEOBJECTDATA obj_data[10];
    DWORD data_size;
    BYTE kbdata[256];

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER, &IID_IDirectInput8A, (LPVOID*)&di);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("test_keyboard_events requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8Create failed: %08x\n", hr);

    hr = IDirectInput8_Initialize(di, GetModuleHandleA(NULL), DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("test_keyboard_events requires dinput8\n");
        IDirectInput8_Release(di);
        return;
    }
    ok(SUCCEEDED(hr), "IDirectInput8_Initialize failed: %08x\n", hr);

    hwnd = CreateWindowExA(WS_EX_TOPMOST, "static", "dinput", WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    ok(hwnd != NULL, "CreateWindowExA failed\n");

    hr = IDirectInput8_CreateDevice(di, &GUID_SysKeyboard, &di_keyboard, NULL);
    ok(SUCCEEDED(hr), "IDirectInput8_CreateDevice failed: %08x\n", hr);
    hr = IDirectInputDevice8_SetCooperativeLevel(di_keyboard, hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    ok(SUCCEEDED(hr), "IDirectInput8_SetCooperativeLevel failed: %08x\n", hr);
    hr = IDirectInputDevice8_SetDataFormat(di_keyboard, &c_dfDIKeyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);
    dp.diph.dwSize = sizeof(DIPROPDWORD);
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwObj = 0;
    dp.diph.dwHow = DIPH_DEVICE;
    dp.dwData = ARRAY_SIZE(obj_data);
    IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_BUFFERSIZE, &(dp.diph));

    hr = IDirectInputDevice8_Acquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);

    /* Test injecting keyboard events with both VK and scancode given. */
    keybd_event(VK_SPACE, DIK_SPACE, 0, 0);
    flush_events();
    IDirectInputDevice8_Poll(di_keyboard);
    data_size = ARRAY_SIZE(obj_data);
    hr = IDirectInputDevice8_GetDeviceData(di_keyboard, sizeof(DIDEVICEOBJECTDATA), obj_data, &data_size, 0);
    ok(SUCCEEDED(hr), "Failed to get data hr=%08x\n", hr);
    ok(data_size == 1, "Expected 1 element, received %d\n", data_size);

    hr = IDirectInputDevice8_GetDeviceState(di_keyboard, sizeof(kbdata), kbdata);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_GetDeviceState failed: %08x\n", hr);
    ok(kbdata[DIK_SPACE], "Expected DIK_SPACE key state down\n");

    keybd_event(VK_SPACE, DIK_SPACE, KEYEVENTF_KEYUP, 0);
    flush_events();
    IDirectInputDevice8_Poll(di_keyboard);
    data_size = ARRAY_SIZE(obj_data);
    hr = IDirectInputDevice8_GetDeviceData(di_keyboard, sizeof(DIDEVICEOBJECTDATA), obj_data, &data_size, 0);
    ok(SUCCEEDED(hr), "Failed to get data hr=%08x\n", hr);
    ok(data_size == 1, "Expected 1 element, received %d\n", data_size);

    /* Test injecting keyboard events with scancode=0.
     * Windows DInput ignores the VK, sets scancode 0 to be pressed, and GetDeviceData returns no elements. */
    keybd_event(VK_SPACE, 0, 0, 0);
    flush_events();
    IDirectInputDevice8_Poll(di_keyboard);
    data_size = ARRAY_SIZE(obj_data);
    hr = IDirectInputDevice8_GetDeviceData(di_keyboard, sizeof(DIDEVICEOBJECTDATA), obj_data, &data_size, 0);
    ok(SUCCEEDED(hr), "Failed to get data hr=%08x\n", hr);
    ok(data_size == 0, "Expected 0 elements, received %d\n", data_size);

    hr = IDirectInputDevice8_GetDeviceState(di_keyboard, sizeof(kbdata), kbdata);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_GetDeviceState failed: %08x\n", hr);
    todo_wine
    ok(kbdata[0], "Expected key 0 state down\n");

    keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0);
    flush_events();
    IDirectInputDevice8_Poll(di_keyboard);
    data_size = ARRAY_SIZE(obj_data);
    hr = IDirectInputDevice8_GetDeviceData(di_keyboard, sizeof(DIDEVICEOBJECTDATA), obj_data, &data_size, 0);
    ok(SUCCEEDED(hr), "Failed to get data hr=%08x\n", hr);
    ok(data_size == 0, "Expected 0 elements, received %d\n", data_size);

    hr = IDirectInputDevice8_Unacquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Unacquire failed: %08x\n", hr);

    IDirectInputDevice8_Release(di_keyboard);
    IDirectInput8_Release(di);

    DestroyWindow(hwnd);
}

static void test_appdata_property(void)
{
    HRESULT hr;
    HINSTANCE hinst = GetModuleHandleA(NULL);
    IDirectInputDevice8A *di_keyboard;
    IDirectInput8A *pDI = NULL;
    HWND hwnd;
    DIPROPDWORD dw;
    DIPROPPOINTER dp;

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER, &IID_IDirectInput8A, (LPVOID*)&pDI);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("DIPROP_APPDATA requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8 Create failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hr = IDirectInput8_Initialize(pDI,hinst, DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("DIPROP_APPDATA requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8 Initialize failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hwnd = CreateWindowExA(WS_EX_TOPMOST, "static", "dinput",
            WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    ok(hwnd != NULL, "failed to create window\n");

    hr = IDirectInput8_CreateDevice(pDI, &GUID_SysKeyboard, &di_keyboard, NULL);
    ok(SUCCEEDED(hr), "IDirectInput8_CreateDevice failed: %08x\n", hr);

    hr = IDirectInputDevice8_SetDataFormat(di_keyboard, &c_dfDIKeyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);

    dw.diph.dwSize = sizeof(DIPROPDWORD);
    dw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dw.diph.dwObj = 0;
    dw.diph.dwHow = DIPH_DEVICE;
    dw.dwData = 32;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_BUFFERSIZE, &(dw.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    /* the default value */
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_A, -1);

    dp.diph.dwHow = DIPH_DEVICE;
    dp.diph.dwObj = 0;
    dp.uData = 1;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty APPDATA for the device should be invalid hr=%08x\n", hr);

    dp.diph.dwSize = sizeof(dp);
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwHow = DIPH_BYUSAGE;
    dp.diph.dwObj = 2;
    dp.uData = 2;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_SetProperty APPDATA by usage should be unsupported hr=%08x\n", hr);

    dp.diph.dwHow = DIPH_BYID;
    dp.diph.dwObj = DIDFT_MAKEINSTANCE(DIK_SPACE) | DIDFT_PSHBUTTON;
    dp.uData = 3;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    dp.diph.dwHow = DIPH_BYOFFSET;
    dp.diph.dwObj = DIK_A;
    dp.uData = 4;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    dp.diph.dwHow = DIPH_BYOFFSET;
    dp.diph.dwObj = DIK_B;
    dp.uData = 5;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_SPACE, 3);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_A, 4);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_B, 5);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_C, -1);

    /* setting data format resets APPDATA */
    hr = IDirectInputDevice8_SetDataFormat(di_keyboard, &c_dfDIKeyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);

    test_device_input(di_keyboard, INPUT_KEYBOARD, VK_SPACE, -1);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_A, -1);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_B, -1);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_C, -1);

    DestroyWindow(hwnd);
    IDirectInputDevice_Release(di_keyboard);
    IDirectInput_Release(pDI);
}

static void check_device(const DIDEVICEINSTANCEW *instance, BOOL is_hid)
{
    GUID guid_null = {0};
    GUID hid_instance = {0x00000000,0x0000,0x11eb,{0x80,0x01,0x44,0x45,0x53,0x54,0x00,0x00}};
    GUID hid_product = {0x00000000,0x0000,0x0000,{0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44}};
    BOOL todo_name = FALSE;

    switch (GET_DIDEVICE_TYPE(instance->dwDevType))
    {
    case DI8DEVTYPE_DEVICE:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) == 0, "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        break;
    case DI8DEVTYPE_MOUSE:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) >= DI8DEVTYPEMOUSE_UNKNOWN &&
            GET_DIDEVICE_SUBTYPE(instance->dwDevType) <= DI8DEVTYPEMOUSE_ABSOLUTE,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        todo_name = TRUE;
        break;
    case DI8DEVTYPE_KEYBOARD:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) >= DI8DEVTYPEKEYBOARD_UNKNOWN &&
            GET_DIDEVICE_SUBTYPE(instance->dwDevType) <= DI8DEVTYPEKEYBOARD_J3100,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        todo_name = TRUE;
        break;
    case DI8DEVTYPE_JOYSTICK:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) >= DI8DEVTYPEJOYSTICK_LIMITED &&
            GET_DIDEVICE_SUBTYPE(instance->dwDevType) <= DI8DEVTYPEJOYSTICK_STANDARD,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        break;
    case DI8DEVTYPE_GAMEPAD:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) >= DI8DEVTYPEGAMEPAD_LIMITED &&
            GET_DIDEVICE_SUBTYPE(instance->dwDevType) <= DI8DEVTYPEGAMEPAD_TILT,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        break;
    case DI8DEVTYPE_DRIVING:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) >= DI8DEVTYPEDRIVING_LIMITED &&
            GET_DIDEVICE_SUBTYPE(instance->dwDevType) <= DI8DEVTYPEDRIVING_HANDHELD,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        break;
    case DI8DEVTYPE_FLIGHT:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) >= DI8DEVTYPEFLIGHT_LIMITED &&
            GET_DIDEVICE_SUBTYPE(instance->dwDevType) <= DI8DEVTYPEFLIGHT_RC,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        break;
    case DI8DEVTYPE_1STPERSON:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) >= DI8DEVTYPE1STPERSON_LIMITED &&
            GET_DIDEVICE_SUBTYPE(instance->dwDevType) <= DI8DEVTYPE1STPERSON_SHOOTER,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        break;
    case DI8DEVTYPE_DEVICECTRL:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) >= DI8DEVTYPEDEVICECTRL_UNKNOWN &&
            GET_DIDEVICE_SUBTYPE(instance->dwDevType) <= DI8DEVTYPEDEVICECTRL_COMMSSELECTION_HARDWIRED,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        break;
    case DI8DEVTYPE_SCREENPOINTER:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) >= DI8DEVTYPESCREENPTR_UNKNOWN &&
            GET_DIDEVICE_SUBTYPE(instance->dwDevType) <= DI8DEVTYPESCREENPTR_TOUCH,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        break;
    case DI8DEVTYPE_REMOTE:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) == DI8DEVTYPEREMOTE_UNKNOWN,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        break;
    case DI8DEVTYPE_SUPPLEMENTAL:
        ok( GET_DIDEVICE_SUBTYPE(instance->dwDevType) >= DI8DEVTYPESUPPLEMENTAL_UNKNOWN &&
            GET_DIDEVICE_SUBTYPE(instance->dwDevType) <= DI8DEVTYPESUPPLEMENTAL_RUDDERPEDALS,
            "unexpected device subtype %02x\n", GET_DIDEVICE_SUBTYPE(instance->dwDevType) );
        break;
    default:
        ok( 0, "unexpected device type %#x\n", instance->dwDevType );
        break;
    }

    if (!is_hid)
    {
        ok( !instance->wUsagePage && !instance->wUsage, "unexpected HID usages %04x:%04x\n", instance->wUsagePage, instance->wUsage );
        ok( IsEqualGUID(&instance->guidProduct, &GUID_SysMouse) || IsEqualGUID(&instance->guidProduct, &GUID_SysKeyboard) ||
            IsEqualGUID(&instance->guidProduct, &GUID_Joystick), "unexpected guidProduct %s\n", debugstr_guid(&instance->guidProduct) );
        ok( IsEqualGUID(&instance->guidProduct, &instance->guidInstance), "unexpected guidProduct %s, guidInstance %s\n", debugstr_guid(&instance->guidProduct), debugstr_guid(&instance->guidInstance) );
    }
    else
    {
        ok( (instance->dwDevType & ~0xffff) == DIDEVTYPE_HID, "unexpected HID device type %x, expected DIDEVTYPE_HID bit set\n", instance->dwDevType );
        ok( instance->wUsagePage && instance->wUsage, "unexpected HID usages %04x:%04x\n", instance->wUsagePage, instance->wUsage );

        hid_product.Data1 = instance->guidProduct.Data1;
        ok( IsEqualGUID(&instance->guidProduct, &hid_product), "unexpected guidProduct %s, expected %s\n", debugstr_guid(&instance->guidProduct), debugstr_guid(&hid_product) );

        hid_instance.Data1 = instance->guidInstance.Data1;
        hid_instance.Data2 = instance->guidInstance.Data2;
        todo_wine ok( IsEqualGUID(&instance->guidInstance, &hid_instance), "unexpected guidInstance %s, expected %s\n", debugstr_guid(&instance->guidInstance), debugstr_guid(&hid_instance) );
    }

    todo_wine_if(todo_name) ok( !wcscmp(instance->tszInstanceName, instance->tszProductName), "unexpected, product name %s, instance name %s\n", debugstr_w(instance->tszProductName), debugstr_w(instance->tszInstanceName) );
    ok( IsEqualGUID(&instance->guidFFDriver, &guid_null), "unexpected guidFFDriver %s\n", debugstr_guid(&instance->guidFFDriver) );
}

static BOOL CALLBACK check_devices_callback(const DIDEVICEINSTANCEW *instance, void *ref)
{
    BOOL is_hid = instance->dwDevType & ~0xffff;
    check_device(instance, is_hid);
    return DIENUM_CONTINUE;
}

static BOOL CALLBACK check_hid_devices_callback(const DIDEVICEINSTANCEW *instance, void *ref)
{
    check_device(instance, TRUE);
    return DIENUM_CONTINUE;
}

static BOOL CALLBACK dump_devices_objects_callback(const DIDEVICEOBJECTINSTANCEW *instance, void *ref)
{
    DIDEVCAPS *dev_caps = ref;

    if (IsEqualGUID(&instance->guidType, &GUID_Key))
    {
        ok(instance->dwOfs >= DIK_ESCAPE && instance->dwOfs <= 0xff, "unexpected dwOfs %#x\n", instance->dwOfs);
        ok(DIDFT_GETINSTANCE(instance->dwType) == instance->dwOfs, "unexpected dwType %#x, expected %#x\n", instance->dwType, DIDFT_MAKEINSTANCE(instance->dwOfs) | DIDFT_PSHBUTTON);
        ok(DIDFT_GETTYPE(instance->dwType) == DIDFT_PSHBUTTON, "unexpected dwType %#x, expected %#x\n", instance->dwType, DIDFT_MAKEINSTANCE(instance->dwOfs) | DIDFT_PSHBUTTON);
        ok(instance->dwFlags == 0, "unexpected dwFlags %#x\n", instance->dwFlags);
        ok(instance->dwFFMaxForce == 0, "unexpected dwFFMaxForce %#x\n", instance->dwFFMaxForce);
        ok(instance->dwFFForceResolution == 0, "unexpected dwFFForceResolution %#x\n", instance->dwFFForceResolution);
        ok(instance->wCollectionNumber == 0, "unexpected wCollectionNumber %04x\n", instance->wCollectionNumber);
        ok(instance->wDesignatorIndex == 0, "unexpected wDesignatorIndex %04x\n", instance->wDesignatorIndex);
        ok(instance->wUsagePage == 0 && instance->wUsage == 0, "unexpected usage %04x:%04x\n", instance->wUsagePage, instance->wUsage);
        ok(instance->dwDimension == 0, "unexpected dwDimension %#x\n", instance->dwDimension);
        ok(instance->wExponent == 0, "unexpected wExponent %04x\n", instance->wExponent);
        ok(instance->wReportId == 0, "unexpected wReportId %04x\n", instance->wReportId);
    }
    else if (IsEqualGUID(&instance->guidType, &GUID_Button))
    {
    }
    else if (IsEqualGUID(&instance->guidType, &GUID_XAxis))
    {
    }
    else if (IsEqualGUID(&instance->guidType, &GUID_YAxis))
    {
    }
    else if (IsEqualGUID(&instance->guidType, &GUID_ZAxis))
    {
    }
    else if (IsEqualGUID(&instance->guidType, &GUID_RxAxis))
    {
    }
    else if (IsEqualGUID(&instance->guidType, &GUID_RyAxis))
    {
    }
    else if (IsEqualGUID(&instance->guidType, &GUID_RzAxis))
    {
    }
    else if (IsEqualGUID(&instance->guidType, &GUID_POV))
    {
    }
    else if (IsEqualGUID(&instance->guidType, &GUID_Unknown))
    {
        ok(instance->dwOfs == 0, "unexpected dwOfs %#x\n", instance->dwOfs);
        ok(DIDFT_GETTYPE(instance->dwType) == (DIDFT_COLLECTION|DIDFT_NODATA), "unexpected dwType %#x, expected %#x\n", instance->dwType, DIDFT_COLLECTION|DIDFT_NODATA);
        ok(instance->dwFlags == 0, "unexpected dwFlags %#x\n", instance->dwFlags);
        ok(instance->dwFFMaxForce == 0, "unexpected dwFFMaxForce %#x\n", instance->dwFFMaxForce);
        ok(instance->dwFFForceResolution == 0, "unexpected dwFFForceResolution %#x\n", instance->dwFFForceResolution);
        ok(instance->wCollectionNumber == 0, "unexpected wCollectionNumber %04x\n", instance->wCollectionNumber);
        ok(instance->wDesignatorIndex == 0, "unexpected wDesignatorIndex %04x\n", instance->wDesignatorIndex);
        ok(instance->wUsagePage != 0 && (instance->wUsage == 5 || instance->wUsage == 0), "unexpected usage %04x:%04x\n", instance->wUsagePage, instance->wUsage);
        ok(instance->dwDimension == 0, "unexpected dwDimension %#x\n", instance->dwDimension);
        ok(instance->wExponent == 0, "unexpected wExponent %04x\n", instance->wExponent);
        ok(instance->wReportId == 0, "unexpected wReportId %04x\n", instance->wReportId);
    }

    ok(1, "%s %4x %4x %4x %04x:%04x %d %d %3x %3x %3x %3x %3x %s\n",
       debugstr_guid(&instance->guidType), instance->dwOfs, instance->dwType, instance->dwFlags,
       instance->wUsagePage, instance->wUsage, instance->wReportId, instance->wCollectionNumber,
       instance->wDesignatorIndex, instance->dwDimension, instance->wExponent,
       instance->dwFFMaxForce, instance->dwFFForceResolution, debugstr_w(instance->tszName));

    if (DIDFT_GETTYPE(instance->dwType) & DIDFT_BUTTON)
    {
        ok( dev_caps->dwButtons, "unexpected spurious button enumerated\n" );
        if (dev_caps->dwButtons) dev_caps->dwButtons--;
    }
    else if (DIDFT_GETTYPE(instance->dwType) & DIDFT_AXIS)
    {
        ok( dev_caps->dwAxes, "unexpected spurious axis enumerated\n" );
        if (dev_caps->dwAxes) dev_caps->dwAxes--;
    }
    else if (DIDFT_GETTYPE(instance->dwType) == DIDFT_POV)
    {
        ok( dev_caps->dwPOVs, "unexpected spurious pov enumerated\n" );
        if (dev_caps->dwPOVs) dev_caps->dwPOVs--;
    }
    else
    {
        ok( !dev_caps->dwButtons && !dev_caps->dwAxes && !dev_caps->dwPOVs, "unexpected collection\n" );
        ok( instance->dwOfs == 0, "unexpected object offset %x\n", instance->dwOfs);
        ok( DIDFT_GETTYPE(instance->dwType) == (DIDFT_COLLECTION|DIDFT_NODATA), "unexpected object type %x\n", instance->dwType);
    }

    return DIENUM_CONTINUE;
}

static BOOL CALLBACK dump_devices_effects_callback(const DIEFFECTINFOW *info, void *ref)
{
    ok(1, "effect guid %s, type %#x, dynamic %#x, static %#x, name %s\n",
       debugstr_guid(&info->guid), info->dwEffType, info->dwDynamicParams, info->dwStaticParams,
       debugstr_w(info->tszName));
    return TRUE;
}

struct dump_devices_params
{
    IDirectInput8W *dinput;
    HWND hwnd;
};

static BOOL CALLBACK dump_devices_callback(const DIDEVICEINSTANCEW *instance, void *ref)
{
    struct dump_devices_params *params = ref;
    IDirectInputDevice8W *device;
    DIEFFECTINFOW effect_info = {sizeof(DIEFFECTINFOW)};
    DIJOYSTATE state;
    DIDEVCAPS dev_caps;
    HRESULT hr;
    DWORD ffstate;

    hr = IDirectInput8_CreateDevice(params->dinput, &instance->guidInstance, &device, NULL);
    ok(hr == S_OK, "IDirectInput8_CreateDevice returned %#x\n", hr);

    trace("device %s\n", debugstr_w(instance->tszInstanceName));
    dev_caps.dwSize = sizeof(dev_caps);
    hr = IDirectInputDevice8_GetCapabilities(device, &dev_caps);
    ok(hr == S_OK, "IDirectInputDevice8_GetCapabilities returned %#x\n", hr);

    ok(1, "flags %#x axes %#x buttons %#x povs %#x\n", dev_caps.dwFlags, dev_caps.dwAxes, dev_caps.dwButtons, dev_caps.dwPOVs);
    ok(dev_caps.dwDevType == instance->dwDevType, "unexpected dwDevType %#x, expected %#x\n", dev_caps.dwDevType, instance->dwDevType);
    ok(dev_caps.dwFFSamplePeriod == 0, "dev_caps dwFFSamplePeriod %#x\n", dev_caps.dwFFSamplePeriod);
    ok(dev_caps.dwFFMinTimeResolution == 0, "dev_caps dwFFMinTimeResolution %#x\n", dev_caps.dwFFMinTimeResolution);
    ok(dev_caps.dwFirmwareRevision == 0, "dev_caps dwFirmwareRevision %#x\n", dev_caps.dwFirmwareRevision);
    ok(dev_caps.dwHardwareRevision == 0, "dev_caps dwHardwareRevision %#x\n", dev_caps.dwHardwareRevision);
    ok(dev_caps.dwFFDriverVersion == 0, "dev_caps dwFFDriverVersion %#x\n", dev_caps.dwFFDriverVersion);

    ok(0, "                                  guid  ofs type   fl page:usge r c idx dim exp fmx frs name\n");
    hr = IDirectInputDevice8_EnumObjects(device, dump_devices_objects_callback, &dev_caps, DIDFT_ALL);
    ok(hr == S_OK, "IDirectInputDevice8_EnumObjects returned %#x\n", hr);

    ok( !dev_caps.dwButtons && !dev_caps.dwAxes && !dev_caps.dwPOVs, "missing objects %d %d %d\n", dev_caps.dwButtons, dev_caps.dwAxes, dev_caps.dwPOVs );

    hr = IDirectInputDevice8_SetDataFormat(device, &c_dfDIJoystick);
    ok(hr == S_OK, "IDirectInputDevice8_SetDataFormat returned %#x\n", hr);

    if (params->hwnd)
    {
        hr = IDirectInputDevice8_SetCooperativeLevel(device, params->hwnd, DISCL_EXCLUSIVE|DISCL_FOREGROUND);
        ok(hr == S_OK, "IDirectInputDevice8_SetCooperativeLevel returned %#x\n", hr);

        hr = IDirectInputDevice8_Acquire(device);
        ok(hr == S_OK, "IDirectInputDevice8_Acquire returned %#x\n", hr);
    }

    hr = IDirectInputDevice8_Poll(device);
    if (!params->hwnd) ok(hr == DIERR_NOTACQUIRED, "IDirectInputDevice8_Poll returned %#x\n", hr);
    else ok(hr == DI_NOEFFECT, "IDirectInputDevice8_Poll returned %#x\n", hr);

    hr = IDirectInputDevice8_GetDeviceState(device, sizeof(state), &state);
    if (!params->hwnd) ok(hr == DIERR_NOTACQUIRED, "IDirectInputDevice8_GetDeviceState returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetDeviceState returned %#x\n", hr);

    hr = IDirectInputDevice8_GetForceFeedbackState(device, &ffstate);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_GetForceFeedbackState returned %#x\n", hr);
    else if (!params->hwnd) ok(hr == DIERR_NOTEXCLUSIVEACQUIRED, "IDirectInputDevice8_GetForceFeedbackState returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetForceFeedbackState returned %#x\n", hr);
    if (!dev_caps.dwFFDriverVersion) ok(ffstate == 0, "IDirectInputDevice8_GetForceFeedbackState returned state %#x\n", ffstate);
    else ok(ffstate == (DIGFFS_USERFFSWITCHON|DIGFFS_SAFETYSWITCHOFF|DIGFFS_POWERON|DIGFFS_ACTUATORSON|DIGFFS_STOPPED|DIGFFS_EMPTY), "IDirectInputDevice8_GetForceFeedbackState returned state %#x\n", ffstate);

#if 0
    hr = IDirectInputDevice8_SendForceFeedbackCommand(device, ffstate);
    ok(hr == S_OK, "IDirectInputDevice8_SendForceFeedbackCommand returned %#x\n", hr);
#endif

    hr = IDirectInputDevice8_EnumEffects(device, dump_devices_effects_callback, NULL, DIEFT_ALL);
    ok(hr == S_OK, "IDirectInputDevice8_EnumEffects returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_ConstantForce);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_ConstantForce returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_RampForce);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_RampForce returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_Square);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_Square returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_Sine);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_Sine returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_Triangle);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_Triangle returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_SawtoothUp);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_SawtoothUp returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_SawtoothDown);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_SawtoothDown returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_Spring);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_Spring returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_Damper);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_Damper returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_Inertia);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_Inertia returned %#x\n", hr);

    hr = IDirectInputDevice8_GetEffectInfo(device, &effect_info, &GUID_Friction);
    if (!dev_caps.dwFFDriverVersion) ok(hr == DIERR_DEVICENOTREG, "IDirectInputDevice8_GetEffectInfo returned %#x\n", hr);
    else ok(hr == S_OK, "IDirectInputDevice8_GetEffectInfo GUID_Friction returned %#x\n", hr);

#if 0
    hr = IDirectInputDevice8_GetProperty(device, REFGUID rguidProp, DIPROPHEADER *pdiph);
    ok(hr == S_OK, "IDirectInputDevice8_GetProperty returned %#x\n", hr);

    hr = IDirectInputDevice8_SetProperty(device, REFGUID rguidProp, const DIPROPHEADER *pdiph);
    ok(hr == S_OK, "IDirectInputDevice8_SetProperty returned %#x\n", hr);

    hr = IDirectInputDevice8_Unacquire(device,);
    ok(hr == S_OK, "IDirectInputDevice8_Unacquire returned %#x\n", hr);

    hr = IDirectInputDevice8_GetDeviceState(device, DWORD cbData, VOID *lpvData);
    ok(hr == S_OK, "IDirectInputDevice8_GetDeviceState returned %#x\n", hr);

    hr = IDirectInputDevice8_GetDeviceData(device, DWORD cbObjectData, DIDEVICEOBJECTDATA *rgdod, DWORD *pdwInOut, DWORD dwFlags);
    ok(hr == S_OK, "IDirectInputDevice8_GetDeviceData returned %#x\n", hr);

    hr = IDirectInputDevice8_SetDataFormat(device, const DIDATAFORMAT *lpdf);
    ok(hr == S_OK, "IDirectInputDevice8_SetDataFormat returned %#x\n", hr);

    hr = IDirectInputDevice8_SetEventNotification(device, HANDLE hEvent);
    ok(hr == S_OK, "IDirectInputDevice8_SetEventNotification returned %#x\n", hr);

    hr = IDirectInputDevice8_SetCooperativeLevel(device, HWND hwnd, DWORD dwFlags);
    ok(hr == S_OK, "IDirectInputDevice8_SetCooperativeLevel returned %#x\n", hr);

    hr = IDirectInputDevice8_GetObjectInfo(device, DIDEVICEOBJECTINSTANCEW *pdidoi, DWORD dwObj, DWORD dwHow);
    ok(hr == S_OK, "IDirectInputDevice8_GetObjectInfo returned %#x\n", hr);

    hr = IDirectInputDevice8_GetDeviceInfo(device, DIDEVICEINSTANCEW *pdidi);
    ok(hr == S_OK, "IDirectInputDevice8_GetDeviceInfo returned %#x\n", hr);

    hr = IDirectInputDevice8_Initialize(device, HINSTANCE hinst, DWORD dwVersion, REFGUID rguid);
    ok(hr == S_OK, "IDirectInputDevice8_Initialize returned %#x\n", hr);

    hr = IDirectInputDevice8_CreateEffect(device, REFGUID rguid, const DIEFFECT *lpeff, DIRECTINPUTEFFECT **ppdeff, UNKNOWN *punkOuter);
    ok(hr == S_OK, "IDirectInputDevice8_CreateEffect returned %#x\n", hr);

    hr = IDirectInputDevice8_EnumCreatedEffectObjects(device, DIENUMCREATEDEFFECTOBJECTSCALLBACK *lpCallback, VOID *pvRef, DWORD fl);
    ok(hr == S_OK, "IDirectInputDevice8_EnumCreatedEffectObjects returned %#x\n", hr);

    hr = IDirectInputDevice8_Escape(device, DIEFFESCAPE *pesc);
    ok(hr == S_OK, "IDirectInputDevice8_Escape returned %#x\n", hr);

    hr = IDirectInputDevice8_SendDeviceData(device, DWORD cbObjectData, const DIDEVICEOBJECTDATA *rgdod, DWORD *pdwInOut, DWORD fl);
    ok(hr == S_OK, "IDirectInputDevice8_SendDeviceData returned %#x\n", hr);

    hr = IDirectInputDevice8_EnumEffectsInFile(device, const WCHAR* lpszFileName,LPDIENUMEFFECTSINFILECALLBACK pec,LPVOID pvRef,DWORD dwFlags);
    ok(hr == S_OK, "IDirectInputDevice8_EnumEffectsInFile returned %#x\n", hr);

    hr = IDirectInputDevice8_WriteEffectToFile(device, const WCHAR* lpszFileName,DWORD dwEntries,LPDIFILEEFFECT rgDiFileEft,DWORD dwFlags);
    ok(hr == S_OK, "IDirectInputDevice8_WriteEffectToFile returned %#x\n", hr);

    hr = IDirectInputDevice8_BuildActionMap(device, DIACTIONFORMATW *lpdiaf, const WCHAR* lpszUserName, DWORD dwFlags);
    ok(hr == S_OK, "IDirectInputDevice8_BuildActionMap returned %#x\n", hr);

    hr = IDirectInputDevice8_SetActionMap(device, DIACTIONFORMATW *lpdiaf, const WCHAR* lpszUserName, DWORD dwFlags);
    ok(hr == S_OK, "IDirectInputDevice8_SetActionMap returned %#x\n", hr);

    hr = IDirectInputDevice8_GetImageInfo(device, DIDEVICEIMAGEINFOHEADERW *lpdiDevImageInfoHeader);
    ok(hr == S_OK, "IDirectInputDevice8_GetImageInfo returned %#x\n", hr);
#endif

    IDirectInputDevice8_Release(device);

    return DIENUM_CONTINUE;
}

static void dump_devices(void)
{
    IDirectInput8W *dinput;
    HRESULT hr;
    HWND hwnd;

    hwnd = CreateWindowExW(WS_EX_TOPMOST, L"static", L"dinput", WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    ok(!!hwnd, "CreateWindowExW failed\n");
    flush_events();

    hr = DirectInput8Create(GetModuleHandleW(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&dinput, NULL);
    ok(hr == S_OK, "DirectInput8Create returned %#x\n", hr);

#if 0
    IDirectInput8_CreateDevice
    IDirectInput8_EnumDevices
    IDirectInput8_FindDevice
    IDirectInput8_EnumDevicesBySemantics
    IDirectInput8_ConfigureDevices
#endif

    hr = IDirectInput8_GetDeviceStatus(dinput, &GUID_SysMouse);
    ok(hr == S_OK, "IDirectInput8_GetDeviceStatus GUID_SysMouse returned %#x\n", hr);
    hr = IDirectInput8_GetDeviceStatus(dinput, &GUID_SysKeyboard);
    ok(hr == S_OK, "IDirectInput8_GetDeviceStatus GUID_SysKeyboard returned %#x\n", hr);
    hr = IDirectInput8_GetDeviceStatus(dinput, &GUID_Joystick);
    todo_wine_if(hr != S_OK) ok(hr == S_OK || hr == REGDB_E_CLASSNOTREG, "IDirectInput8_GetDeviceStatus GUID_Joystick returned %#x\n", hr);
    hr = IDirectInput8_GetDeviceStatus(dinput, &GUID_SysMouseEm);
    ok(hr == S_FALSE, "IDirectInput8_GetDeviceStatus GUID_SysMouseEm returned %#x\n", hr);
    hr = IDirectInput8_GetDeviceStatus(dinput, &GUID_SysMouseEm2);
    ok(hr == S_FALSE, "IDirectInput8_GetDeviceStatus GUID_SysMouseEm2 returned %#x\n", hr);
    hr = IDirectInput8_GetDeviceStatus(dinput, &GUID_SysKeyboardEm);
    ok(hr == S_FALSE, "IDirectInput8_GetDeviceStatus GUID_SysKeyboardEm returned %#x\n", hr);
    hr = IDirectInput8_GetDeviceStatus(dinput, &GUID_SysKeyboardEm2);
    ok(hr == S_FALSE, "IDirectInput8_GetDeviceStatus GUID_SysKeyboardEm2 returned %#x\n", hr);


    hr = IDirectInput8_EnumDevices(dinput, DI8DEVCLASS_ALL, check_devices_callback, hwnd, DIEDFL_ALLDEVICES);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, DI8DEVCLASS_DEVICE, check_devices_callback, hwnd, DIEDFL_ALLDEVICES);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, DI8DEVCLASS_POINTER, check_devices_callback, hwnd, DIEDFL_ALLDEVICES);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, DI8DEVCLASS_KEYBOARD, check_devices_callback, hwnd, DIEDFL_ALLDEVICES);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, DI8DEVCLASS_GAMECTRL, check_devices_callback, hwnd, DIEDFL_ALLDEVICES);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, DIDEVTYPE_HID, check_hid_devices_callback, hwnd, DIEDFL_ALLDEVICES);
    todo_wine ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, 5, check_devices_callback, hwnd, DIEDFL_ALLDEVICES);
    ok(hr == DIERR_INVALIDPARAM, "IDirectInput8_EnumDevices returned %#x\n", hr);

    struct dump_devices_params params = {dinput, hwnd};
    hr = IDirectInput8_EnumDevices(dinput, DI8DEVCLASS_ALL, dump_devices_callback, &params, DIEDFL_ALLDEVICES);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);

#if 0
    hr = IDirectInput8_EnumDevices(dinput, 0, dump_devices_callback, NULL, DIEDFL_ALLDEVICES);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, 0, dump_devices_callback, NULL, DIEDFL_ATTACHEDONLY);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, 0, dump_devices_callback, NULL, DIEDFL_FORCEFEEDBACK);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, 0, dump_devices_callback, NULL, DIEDFL_INCLUDEALIASES);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, 0, dump_devices_callback, NULL, DIEDFL_INCLUDEPHANTOMS);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
    hr = IDirectInput8_EnumDevices(dinput, 0, dump_devices_callback, NULL, DIEDFL_INCLUDEHIDDEN);
    ok(hr == S_OK, "IDirectInput8_EnumDevices returned %#x\n", hr);
#endif

    /* hr = IDirectInput8_FindDevice(p,a,b,c); */

    IDirectInput8_Release(dinput);

    DestroyWindow(hwnd);
}

START_TEST(device)
{
    CoInitialize(NULL);

    dump_devices();
#if 0
    test_action_mapping();
    test_save_settings();
    test_mouse_keyboard();
    test_keyboard_events();
    test_appdata_property();
#endif

    CoUninitialize();
}
