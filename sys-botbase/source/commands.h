#include <switch.h>

extern Handle debughandle;
extern bool bControllerIsInitialised;
extern HiddbgHdlsHandle controllerHandle;
extern HiddbgHdlsDeviceInfo controllerDevice;
extern HiddbgHdlsState controllerState;
extern HiddbgKeyboardAutoPilotState dummyKeyboardState;
extern u64 buttonClickSleepTime;
extern u64 keyPressSleepTime;
extern u64 pollRate;
extern u32 fingerDiameter;

typedef struct {
    u64 main_nso_base;
    u64 heap_base;
    u64 titleID;
    u8 buildID[0x20];
} MetaData;

typedef struct {
    HidTouchState* states;
    u64 sequentialCount;
    u64 holdTime;
    bool hold;
    u8 state;
} TouchData;

typedef struct {
    HiddbgKeyboardAutoPilotState* states;
    u64 sequentialCount;
    u8 state;
} KeyData;

struct ResponseHandler;

#define JOYSTICK_LEFT 0
#define JOYSTICK_RIGHT 1

void attach(struct ResponseHandler *response);
void detach();
void detachController(struct ResponseHandler *response);
u64 getMainNsoBase(u64 pid, struct ResponseHandler *response);
u64 getHeapBase(Handle handle);
u64 getTitleId(u64 pid, struct ResponseHandler *response);
void getBuildID(MetaData* meta, u64 pid, struct ResponseHandler *response);
MetaData getMetaData(struct ResponseHandler *response);

void poke(u64 offset, u64 size, u8* val, struct ResponseHandler *response);
void writeMem(u64 offset, u64 size, u8* val, struct ResponseHandler *response);
void peek(u64 offset, u64 size, struct ResponseHandler *response);
void readMem(u8* out, u64 offset, u64 size, struct ResponseHandler *response);
void click(HidNpadButton btn, struct ResponseHandler *response);
void press(HidNpadButton btn, struct ResponseHandler *response);
void release(HidNpadButton btn, struct ResponseHandler *response);
void setStickState(int side, int dxVal, int dyVal, struct ResponseHandler *response);
void reverseArray(u8* arr, int start, int end);
u64 followMainPointer(s64* jumps, size_t count, struct ResponseHandler *response);
void touch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold, u8* token, struct ResponseHandler *response);
void key(HiddbgKeyboardAutoPilotState* states, u64 sequentialCount, struct ResponseHandler *response);
void clickSequence(char* seq, u8* token, struct ResponseHandler *response);