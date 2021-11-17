#include <switch.h>
#define MAX_LINE_LENGTH 344 * 32 * 2

extern u64 mainLoopSleepTime;
extern bool debugResultCodes;

struct ResponseHandler
{
	void (*response)(struct ResponseHandler *handler, void *buffer, u32 bufferSize);
	void (*responsePrintf)(struct ResponseHandler *handler, char *fmt, ...);
	void *responseData;
	int  (*argmain)(int , char **, struct ResponseHandler *);
};

int setupServerSocket();
u64 parseStringToInt(char* arg);
s64 parseStringToSignedLong(char* arg);
u8* parseStringToByteBuffer(char* arg, u64* size);
HidNpadButton parseStringToButton(char* arg);
Result capsscCaptureForDebug(void *buffer, size_t buffer_size, u64 *size); //big thanks to Behemoth from the Reswitched Discord!
void flashLed(void);