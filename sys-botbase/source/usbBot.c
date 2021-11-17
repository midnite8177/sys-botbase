#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <switch.h>
#include <stdarg.h>
#include "usbBot.h"
#include "args.h"
#include "util.h"

#define THREAD_SIZE 0x20000

struct USBResponse
{
    u32 size;
    u32 _pad;
    void* data;
};

void usbBotWorkerThread(void *arg);
int    usbBotWorkerThreadState = 0;
Mutex  usbBotWorkerThreadMutex;
Thread usbBotWorkerThreadHandle;

void sendUsbResponse(struct USBResponse *response)
{
    usbCommsWrite((void*)&response->size, 4);

    if (response->size > 0)
        usbCommsWrite(response->data, response->size);
}

void usbBotFlushResponse(struct ResponseHandler *handler)
{
	struct USBResponse *response = (struct USBResponse*)handler->responseData;
	if( response == NULL || response->size == 0 )
		return;

	sendUsbResponse( response );

	if( response->data != NULL )
	{
		free( response->data );
		response->data = NULL;
	}

	response->size = 0;
}

void usbBotResponse(struct ResponseHandler *handler, void *buffer, u32 bufferSize)
{
	if( bufferSize == 0 || buffer == NULL || handler == NULL )
		return;

	struct USBResponse *response = (struct USBResponse *)handler->responseData;

	// Append to existing buffer
	if(response->size == 0)
	{
		response->data = malloc( bufferSize );
		if( response->data )
		{
			memcpy( response->data, buffer, bufferSize );
			response->size = bufferSize;
		}
		else
		{
			// Out of memory
			response->size = 0;
		}
	}
	else
	{
		u32 newSize = response->size + bufferSize;
		response->data = realloc( response->data, newSize );
		if( response->data )
		{
			memcpy( ((char*)response->data) + response->size, buffer, bufferSize );
			response->size = newSize;
		}
		else
		{
			// Out of memory
			response->size = 0;
		}
	}
}

void usbBotResponsePrintf(struct ResponseHandler *handler, char *fmt, ...)
{
    va_list args1;
    va_start(args1, fmt);
    va_list args2;
    va_copy(args2, args1);
    int stringLen = vsnprintf(NULL, 0, fmt, args1);
    char *buffer = (char *)malloc(stringLen + 1);
    va_end(args1);
    vsnprintf(buffer, stringLen + 1, fmt, args2);
    va_end(args2);

    usbBotResponse(handler, buffer, stringLen);

    free(buffer);
}

Result usbBotInitialize(struct ResponseHandler *handler, int (*argmain)(int , char **, struct ResponseHandler *))
{
	Result rc = usbCommsInitialize();
	if( R_FAILED(rc) )
		return rc;

	// setup the response handler
	handler->response       = usbBotResponse;
	handler->responsePrintf = usbBotResponsePrintf;
	handler->responseData   = calloc( 1, sizeof(struct USBResponse) );
	handler->argmain        = argmain;

	// initialize the worker thread
	usbBotWorkerThreadState = 0;
    mutexInit( &usbBotWorkerThreadMutex );
    rc = threadCreate( &usbBotWorkerThreadHandle, usbBotWorkerThread, (void*)handler, NULL, THREAD_SIZE, 0x2C, -2 ); 
    if( R_SUCCEEDED(rc) )
    {
        rc = threadStart( &usbBotWorkerThreadHandle );
    }

	return rc;
}

void usbBotShutdown(struct ResponseHandler *handler)
{
	usbBotWorkerThreadState = 1;
	usbCommsExit();
	threadWaitForExit( &usbBotWorkerThreadHandle );
	threadClose( &usbBotWorkerThreadHandle );

	free(handler->responseData);
	handler->response       = NULL;
	handler->responsePrintf = NULL;
	handler->responseData   = NULL;
	handler->argmain        = NULL;
}

void usbBotWorkerThread(void *arg)
{
	struct ResponseHandler *handler = (struct ResponseHandler *)arg;

	while( 1 )
	{
        if( usbBotWorkerThreadState != 0 )
			break;

		// How big is the payload?
		u32 len;
        size_t res = usbCommsRead( &len, sizeof(len) );
	    if( res != sizeof(len) )
		{
	        // USB transfer failure.
	        svcSleepThread(1e+6L);
	        continue;
	    }

	    if( len > 0 )
	    {
		    // Allocate for the payload (and extra for the line-ending)
		    char *buffer = (char *)malloc( len + 3 );
		    if( buffer == NULL )
		    {
		    	// ERROR: Out of memory
		    	break;
		    }

	    	// Read in the payload
		    res = usbCommsRead( buffer, len );
		    if( res != len )
			{
		        // USB transfer failure.
		        free(buffer);
		        svcSleepThread(1e+6L);
		        continue;
		    }

			// Append a newline and terminator so parseArgs can handle it correctly
			buffer[len + 0] = '\r';
		    buffer[len + 1] = '\n';
		    buffer[len + 2] = '\0';

		    parseArgs( buffer, handler->argmain, handler );

		    free( buffer );

		    // multiple writes can happen, condense them and write them out in one shot
		    usbBotFlushResponse( handler );
		}

		svcSleepThread(1e+6L);
	}
}
