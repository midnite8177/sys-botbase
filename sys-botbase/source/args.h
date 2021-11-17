struct ResponseHandler;

int parseArgs(char* argstr, int (*callback)(int, char**, struct ResponseHandler *), struct ResponseHandler *response);
