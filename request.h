#ifndef __REQUEST_H__

void requestHandle(int fd);
void requestHandleFifo(int fd, suseconds_t _statReqArrival, suseconds_t _statReqDispatch, int _age, int _id, int _reqHandld, int _statReq, int _dynReq);
void requestHandleSff(int fd, int isStatic, int fileSize, int modeError, char* cgiargs, char* method, char* uri, char* version, char* filename,suseconds_t _statReqArrival, suseconds_t _statReqDispatch, int _age, int _id, int _reqHandld, int _statReq, int _dynReq); 
void findReqSize(int fd, int* isStatic, int* fileSize, int* modeErr, char* cgiargs, char* method, char* uri, char* version, char* filename);


#endif
