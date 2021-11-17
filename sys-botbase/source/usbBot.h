#ifndef USB_SYSBOTBASE_H_
#define USB_SYSBOTBASE_H_

#include <switch.h>

struct ResponseHandler;

Result usbBotInitialize(struct ResponseHandler *handler, int (*argmain)(int , char **, struct ResponseHandler *));
void usbBotShutdown(struct ResponseHandler *handler);


#endif
