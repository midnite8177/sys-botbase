#include <switch.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "commands.h"
#include "util.h"

//Controller:
bool bControllerIsInitialised = false;
HiddbgHdlsHandle controllerHandle = {0};
HiddbgHdlsDeviceInfo controllerDevice = {0};
HiddbgHdlsState controllerState = {0};

//Keyboard:
HiddbgKeyboardAutoPilotState dummyKeyboardState = {0};

Handle debughandle = 0;
u64 buttonClickSleepTime = 50;
u64 keyPressSleepTime = 25;
u64 pollRate = 17; // polling is linked to screen refresh rate (system UI) or game framerate. Most cases this is 1/60 or 1/30
u32 fingerDiameter = 50;
HiddbgHdlsSessionId sessionId = {0};

void initController(struct ResponseHandler *response);

void attach(struct ResponseHandler *response)
{
    u64 pid = 0;
    Result rc = pmdmntGetApplicationProcessId(&pid);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "pmdmntGetApplicationProcessId: %d\n", rc);

    if (debughandle != 0)
        svcCloseHandle(debughandle);

    rc = svcDebugActiveProcess(&debughandle, pid);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "svcDebugActiveProcess: %d\n", rc);
}

void detach(){
    if (debughandle != 0)
        svcCloseHandle(debughandle);
}

void detachController(struct ResponseHandler *response)
{
    initController( response );

    Result rc = hiddbgDetachHdlsVirtualDevice(controllerHandle);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "hiddbgDetachHdlsVirtualDevice: %d\n", rc);
    rc = hiddbgReleaseHdlsWorkBuffer(sessionId);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "hiddbgReleaseHdlsWorkBuffer: %d\n", rc);
    hiddbgExit();
    bControllerIsInitialised = false;

    sessionId.id = 0;
}

u64 getMainNsoBase(u64 pid, struct ResponseHandler *response)
{
    LoaderModuleInfo proc_modules[2];
    s32 numModules = 0;
    Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "ldrDmntGetProcessModuleInfo: %d\n", rc);

    LoaderModuleInfo *proc_module = 0;
    if(numModules == 2){
        proc_module = &proc_modules[1];
    }else{
        proc_module = &proc_modules[0];
    }
    return proc_module->base_address;
}

u64 getHeapBase(Handle handle)
{
    MemoryInfo meminfo;
    memset(&meminfo, 0, sizeof(MemoryInfo));
    u64 heap_base = 0;
    u64 lastaddr = 0;
    do
    {
        lastaddr = meminfo.addr;
        u32 pageinfo;
        svcQueryDebugProcessMemory(&meminfo, &pageinfo, handle, meminfo.addr + meminfo.size);
        if((meminfo.type & MemType_Heap) == MemType_Heap){
            heap_base = meminfo.addr;
            break;
        }
    } while (lastaddr < meminfo.addr + meminfo.size);

    return heap_base;
}

u64 getTitleId(u64 pid, struct ResponseHandler *response)
{
    u64 titleId = 0;
    Result rc = pminfoGetProgramId(&titleId, pid);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "pminfoGetProgramId: %d\n", rc);
    return titleId;
}

void getBuildID(MetaData* meta, u64 pid, struct ResponseHandler *response)
{
    LoaderModuleInfo proc_modules[2];
    s32 numModules = 0;
    Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "ldrDmntGetProcessModuleInfo: %d\n", rc);

    LoaderModuleInfo *proc_module = 0;
    if(numModules == 2){
        proc_module = &proc_modules[1];
    }else{
        proc_module = &proc_modules[0];
    }
    memcpy(meta->buildID, proc_module->build_id, 0x20);
}

MetaData getMetaData(struct ResponseHandler *response)
{
    MetaData meta;
    attach(response);
    u64 pid = 0;    
    Result rc = pmdmntGetApplicationProcessId(&pid);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "pmdmntGetApplicationProcessId: %d\n", rc);
    
    meta.main_nso_base = getMainNsoBase(pid, response);
    meta.heap_base =  getHeapBase(debughandle);
    meta.titleID = getTitleId(pid, response);
    getBuildID(&meta, pid, response);

    detach();
    return meta;
}

void initController(struct ResponseHandler *response)
{
    if(bControllerIsInitialised) return;
    //taken from switchexamples github
    Result rc = hiddbgInitialize();
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "hiddbgInitialize: %d\n", rc);
    // Set the controller type to Pro-Controller, and set the npadInterfaceType.
    controllerDevice.deviceType = HidDeviceType_FullKey3;
    controllerDevice.npadInterfaceType = HidNpadInterfaceType_Bluetooth;
    // Set the controller colors. The grip colors are for Pro-Controller on [9.0.0+].
    controllerDevice.singleColorBody = RGBA8_MAXALPHA(255,255,255);
    controllerDevice.singleColorButtons = RGBA8_MAXALPHA(0,0,0);
    controllerDevice.colorLeftGrip = RGBA8_MAXALPHA(230,255,0);
    controllerDevice.colorRightGrip = RGBA8_MAXALPHA(0,40,20);

    // Setup example controller state.
    controllerState.battery_level = 4; // Set battery charge to full.
    controllerState.analog_stick_l.x = 0x0;
    controllerState.analog_stick_l.y = -0x0;
    controllerState.analog_stick_r.x = 0x0;
    controllerState.analog_stick_r.y = -0x0;
    rc = hiddbgAttachHdlsWorkBuffer(&sessionId);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "hiddbgAttachHdlsWorkBuffer: %d\n", rc);
    rc = hiddbgAttachHdlsVirtualDevice(&controllerHandle, &controllerDevice);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "hiddbgAttachHdlsVirtualDevice: %d\n", rc);
    //init a dummy keyboard state for assignment between keypresses
    dummyKeyboardState.keys[3] = 0x800000000000000UL; // Hackfix found by Red: an unused key press (KBD_MEDIA_CALC) is required to allow sequential same-key presses. bitfield[3]
    bControllerIsInitialised = true;
}

void poke(u64 offset, u64 size, u8* val, struct ResponseHandler *response)
{
    attach(response);
    writeMem(offset, size, val, response);
    detach();
}

void writeMem(u64 offset, u64 size, u8* val, struct ResponseHandler *response)
{
	Result rc = svcWriteDebugProcessMemory(debughandle, val, offset, size);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "svcWriteDebugProcessMemory: %d\n", rc);
}

void peek(u64 offset, u64 size, struct ResponseHandler *response)
{
    u8 *out = malloc(sizeof(u8) * size);
    attach(response);
    readMem(out, offset, size, response);
    detach();

    u64 i;
    for (i = 0; i < size; i++)
    {
        response->responsePrintf(response, "%02X", out[i]);
    }
    response->responsePrintf(response, "\n");
    free(out);
}

void readMem(u8* out, u64 offset, u64 size, struct ResponseHandler *response)
{
	Result rc = svcReadDebugProcessMemory(out, debughandle, offset, size);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "svcReadDebugProcessMemory: %d\n", rc);
}

void click(HidNpadButton btn, struct ResponseHandler *response)
{
    initController(response);
    press(btn, response);
    svcSleepThread(buttonClickSleepTime * 1e+6L);
    release(btn, response);
}
void press(HidNpadButton btn, struct ResponseHandler *response)
{
    initController(response);
    controllerState.buttons |= btn;
    Result rc = hiddbgSetHdlsState(controllerHandle, &controllerState);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "hiddbgSetHdlsState: %d\n", rc);
}

void release(HidNpadButton btn, struct ResponseHandler *response)
{
    initController(response);
    controllerState.buttons &= ~btn;
    Result rc = hiddbgSetHdlsState(controllerHandle, &controllerState);
    if (R_FAILED(rc) && debugResultCodes && response)
        response->responsePrintf(response, "hiddbgSetHdlsState: %d\n", rc);
}

void setStickState(int side, int dxVal, int dyVal, struct ResponseHandler *response)
{
    initController(response);
    if (side == JOYSTICK_LEFT)
    {	
        controllerState.analog_stick_l.x = dxVal;
		controllerState.analog_stick_l.y = dyVal;
	}
	else
	{
		controllerState.analog_stick_r.x = dxVal;
		controllerState.analog_stick_r.y = dyVal;
	}
    hiddbgSetHdlsState(controllerHandle, &controllerState);
}

void reverseArray(u8* arr, int start, int end)
{
    int temp;
    while (start < end)
    {
        temp = arr[start];   
        arr[start] = arr[end];
        arr[end] = temp;
        start++;
        end--;
    }   
} 

u64 followMainPointer(s64* jumps, size_t count, struct ResponseHandler *response) 
{
	u64 offset;
    u64 size = sizeof offset;
	u8 *out = malloc(size);
	MetaData meta = getMetaData(response); 
	
	attach(response);
	readMem(out, meta.main_nso_base + jumps[0], size, response);
	offset = *(u64*)out;
	int i;
    for (i = 1; i < count; ++i)
	{
		readMem(out, offset + jumps[i], size, response);
		offset = *(u64*)out;
	}
	detach();
	free(out);
	
    return offset;
}

void touch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold, u8* token, struct ResponseHandler *response)
{
    initController(response);
    state->delta_time = holdTime; // only the first touch needs this for whatever reason
    for (u32 i = 0; i < sequentialCount; i++)
    {
        hiddbgSetTouchScreenAutoPilotState(&state[i], 1);
        svcSleepThread(holdTime);
        if (!hold)
        {
            hiddbgSetTouchScreenAutoPilotState(NULL, 0);
            svcSleepThread(pollRate * 1e+6L);
        }

        if ((*token) == 1)
            break;
    }

    if(hold) // send finger release event
    {
        hiddbgSetTouchScreenAutoPilotState(NULL, 0);
        svcSleepThread(pollRate * 1e+6L);
    }
    
    hiddbgUnsetTouchScreenAutoPilotState();
}

void key(HiddbgKeyboardAutoPilotState* states, u64 sequentialCount, struct ResponseHandler *response)
{
    initController(response);
    HiddbgKeyboardAutoPilotState tempState = {0};
    u32 i;
    for (i = 0; i < sequentialCount; i++)
    {
        memcpy(&tempState.keys, states[i].keys, sizeof(u64) * 4);
        tempState.modifiers = states[i].modifiers;
        hiddbgSetKeyboardAutoPilotState(&tempState);
        svcSleepThread(keyPressSleepTime * 1e+6L);

        if (i != (sequentialCount-1))
        {
            if (memcmp(states[i].keys, states[i+1].keys, sizeof(u64) * 4) == 0 && states[i].modifiers == states[i+1].modifiers)
            {
                hiddbgSetKeyboardAutoPilotState(&dummyKeyboardState);
                svcSleepThread(pollRate * 1e+6L);
            }
        }
        else
        {
            hiddbgSetKeyboardAutoPilotState(&dummyKeyboardState);
            svcSleepThread(pollRate * 1e+6L);
        }
    }

    hiddbgUnsetKeyboardAutoPilotState();
}

void clickSequence(char* seq, u8* token, struct ResponseHandler *response)
{
    const char delim = ','; // used for chars and sticks
    const char startWait = 'W';
    const char startPress = '+';
    const char startRelease = '-';
    const char startLStick = '%';
    const char startRStick = '&';
    char* command = strtok(seq, &delim);
    HidNpadButton currKey = {0};
    u64 currentWait = 0;

    initController(response);
    while (command != NULL)
    {
        if ((*token) == 1)
            break;

        if (!strncmp(command, &startLStick, 1))
        {
            // l stick
            s64 x = parseStringToSignedLong(&command[1]);
            if(x > JOYSTICK_MAX) x = JOYSTICK_MAX; 
            if(x < JOYSTICK_MIN) x = JOYSTICK_MIN; 
            s64 y = 0;
            command = strtok(NULL, &delim);
            if (command != NULL)
                y = parseStringToSignedLong(command);
            if(y > JOYSTICK_MAX) y = JOYSTICK_MAX;
            if(y < JOYSTICK_MIN) y = JOYSTICK_MIN;
            setStickState(JOYSTICK_LEFT, (s32)x, (s32)y, response);
        }
        else if (!strncmp(command, &startRStick, 1))
        {
            // r stick
            s64 x = parseStringToSignedLong(&command[1]);
            if(x > JOYSTICK_MAX) x = JOYSTICK_MAX; 
            if(x < JOYSTICK_MIN) x = JOYSTICK_MIN; 
            s64 y = 0;
            command = strtok(NULL, &delim);
            if (command != NULL)
                y = parseStringToSignedLong(command);
            if(y > JOYSTICK_MAX) y = JOYSTICK_MAX;
            if(y < JOYSTICK_MIN) y = JOYSTICK_MIN;
            setStickState(JOYSTICK_RIGHT, (s32)x, (s32)y, response);
        }
        else if (!strncmp(command, &startPress, 1))
        {
            // press
            currKey = parseStringToButton(&command[1]);
            press(currKey, response);
        }  
        else if (!strncmp(command, &startRelease, 1))
        {
            // release
            currKey = parseStringToButton(&command[1]);
            press(currKey, response);
        }   
        else if (!strncmp(command, &startWait, 1))
        {
            // wait
            currentWait = parseStringToInt(&command[1]);
            svcSleepThread(currentWait * 1e+6l);
        }
        else
        {
            // click
            currKey = parseStringToButton(command);
            press(currKey, response);
            svcSleepThread(buttonClickSleepTime * 1e+6L);
            release(currKey, response);
        }

        command = strtok(NULL, &delim);
    }
}