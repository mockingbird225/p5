//
// request.c: Does the bulk of the work for the web server.
// 

#include "cs537.h"
#include "request.h"

suseconds_t statReqArrival;
suseconds_t statReqDispatch;
suseconds_t statReqRead;
suseconds_t statReqComplete;
struct timeval tv;
int age;
int thdId, reqHandld, statReq, dynReq;
void requestError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];
    
    printf("Request ERROR\n");
    
    // Create the body of the error message
    sprintf(body, "<html><title>CS537 Error</title>");
    sprintf(body, "%s<body bgcolor=""fffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr>CS537 Web Server\r\n", body);
    
    // Write out the header information for this response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);
    
    sprintf(buf, "Content-Type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);
    
    sprintf(buf, "Content-Length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);
    
    // Write out the content
    Rio_writen(fd, body, strlen(body));
    printf("%s", body);
    
}


//
// Reads and discards everything up to an empty text line
//
void requestReadhdrs(rio_t *rp)
{
    char buf[MAXLINE];
    
    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
	Rio_readlineb(rp, buf, MAXLINE);
    }
    return;
}

//
// Return 1 if static, 0 if dynamic content
// Calculates filename (and cgiargs, for dynamic) from uri
//
int requestParseURI(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;
    
    if (!strstr(uri, "cgi")) {
	// static
	strcpy(cgiargs, "");
	sprintf(filename, ".%s", uri);
	if (uri[strlen(uri)-1] == '/') {
	    strcat(filename, "home.html");
	}
	return 1;
    } else {
	// dynamic
	ptr = index(uri, '?');
	if (ptr) {
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
	} else {
	    strcpy(cgiargs, "");
	}
	sprintf(filename, ".%s", uri);
	return 0;
    }
}

//
// Fills in the filetype given the filename
//
void requestGetFiletype(char *filename, char *filetype)
{
    if (strstr(filename, ".html")) 
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif")) 
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg")) 
	strcpy(filetype, "image/jpeg");
    else 
	strcpy(filetype, "test/plain");
}

void requestServeDynamic(int fd, char *filename, char *cgiargs)
{
	char buf[MAXLINE], *emptylist[] = {NULL};
	// The server does only a little bit of the header.  
	// The CGI script has to finish writing out the header.
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%s Server: Tiny Web Server\r\n", buf);

	/* CS537: Your statistics go here -- fill in the 0's with something useful! */
	sprintf(buf, "%s Stat-req-arrival: %d\r\n", buf, (int)statReqArrival);
	sprintf(buf, "%s Stat-req-dispatch: %d\r\n", buf, (int)statReqDispatch);
	sprintf(buf, "%s Stat-req-age: %d\r\n", buf, age);
	sprintf(buf, "%s Stat-thread-id: %d\r\n", buf, thdId);
	sprintf(buf, "%s Stat-thread-count: %d\r\n", buf, reqHandld);
	sprintf(buf, "%s Stat-thread-static: %d\r\n", buf, statReq);
	sprintf(buf, "%s Stat-thread-dynamic: %d\r\n", buf, dynReq);

	Rio_writen(fd, buf, strlen(buf));

	if (Fork() == 0) {
	/* Child process */
	Setenv("QUERY_STRING", cgiargs, 1);
	/* When the CGI process writes to stdout, it will instead go to the socket */
	Dup2(fd, STDOUT_FILENO);
	Execve(filename, emptylist, environ);
	}
	Wait(NULL);
}


void requestServeStatic(int fd, char *filename, int filesize) 
{
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];
	char tmp = 0;
	int i;
	suseconds_t start, finish, finishWrite;
	requestGetFiletype(filename, filetype);
	
	gettimeofday(&tv, NULL);
	start = (tv.tv_usec)*1000;
	srcfd = Open(filename, O_RDONLY, 0);

	// Rather than call read() to read the file into memory, 
	// which would require that we allocate a buffer, we memory-map the file
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	Close(srcfd);

	// The following code is only needed to help you time the "read" given 
	// that the file is memory-mapped.  
	// This code ensures that the memory-mapped file is brought into memory 
	// from disk.

	// When you time this, you will see that the first time a client 
	//requests a file, the read is much slower than subsequent requests.
	for (i = 0; i < filesize; i++) {
	tmp += *(srcp + i);
	}
	gettimeofday(&tv, NULL);
	finish = (tv.tv_usec)*1000;
	statReqRead = finish-start;

	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%s Server: CS537 Web Server\r\n", buf);

	gettimeofday(&tv, NULL);
	finishWrite = (tv.tv_usec)*1000;	
	statReqComplete = finishWrite-statReqArrival;
	// CS537: Your statistics go here -- fill in the 0's with something useful!
	sprintf(buf, "%s Stat-req-arrival: %d\r\n", buf, (int)statReqArrival);
	sprintf(buf, "%s Stat-req-dispatch: %d\r\n", buf, (int)statReqDispatch);
	sprintf(buf, "%s Stat-req-read: %d\r\n", buf, (int)statReqRead);
	sprintf(buf, "%s Stat-req-complete: %d\r\n", buf, (int)statReqComplete);
	sprintf(buf, "%s Stat-req-age:%d\r\n", buf, age);
	sprintf(buf, "%s Stat-thread-id: %d\r\n", buf, thdId);
	sprintf(buf, "%s Stat-thread-count: %d\r\n", buf, reqHandld);
	sprintf(buf, "%s Stat-thread-static: %d\r\n", buf, statReq);
	sprintf(buf, "%s Stat-thread-dynamic: %d\r\n", buf, dynReq);

	sprintf(buf, "%s Content-Length: %d\r\n", buf, filesize);
	sprintf(buf, "%s Content-Type: %s\r\n\r\n", buf, filetype);

	Rio_writen(fd, buf, strlen(buf));

	//  Writes out to the client socket the memory-mapped file 
	Rio_writen(fd, srcp, filesize);
	Munmap(srcp, filesize);

}

void requestHandleFifo(int fd, suseconds_t _statReqArrival, suseconds_t _statReqDispatch, int _age, int _id, int _reqHandld, int _statReq, int _dynReq)
{
	int is_static;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	statReqArrival = _statReqArrival;
	statReqDispatch = _statReqDispatch;
	age = _age;
	thdId = _id;
	reqHandld = _reqHandld;
	statReq = _statReq;
	dynReq = _dynReq;	
	rio_t rio;
	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);
	printf("%s %s %s\n", method, uri, version);
	if (strcasecmp(method, "GET")) {
		requestError(fd, method, "501", "Not Implemented",
			     "CS537 Server does not implement this method");
		return;
	}
	requestReadhdrs(&rio);

	is_static = requestParseURI(uri, filename, cgiargs);
	if (stat(filename, &sbuf) < 0) {
		requestError(fd, filename, "404", "Not found", "CS537 Server could not find this file");
		return;
	}
	if (is_static) {
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
		    requestError(fd, filename, "403", "Forbidden", "CS537 Server could not read this file");
		    return;
		}
	requestServeStatic(fd, filename, sbuf.st_size);
	} else {
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
		    requestError(fd, filename, "403", "Forbidden", "CS537 Server could not run this CGI program");
		    return;
		}
		requestServeDynamic(fd, filename, cgiargs);
	}
}


// handle a request
void findReqSize(int fd, int* isStatic, int* fileSize, int* modeErr, char* cgiargs, char* method, char* uri, char* version, char* filename) {
	int _isStatic;
	rio_t rio;
	struct stat _sbuf;
	char _filename[MAXLINE], _cgiargs[MAXLINE], _buf[MAXLINE];
	char _method[MAXLINE], _uri[MAXLINE], _version[MAXLINE];
	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, _buf, MAXLINE);
	sscanf(_buf, "%s %s %s", _method, _uri, _version);
	strcpy(method, _method);
	strcpy(uri, _uri);
	strcpy(version, _version);
	if (strcasecmp(method, "GET")) {
		requestError(fd, method, "501", "Not Implemented", 
			     "CS537 Server does not implement this method");
		return;
	}
	requestReadhdrs(&rio);
	_isStatic = requestParseURI(_uri, _filename, _cgiargs);
	strcpy(cgiargs, _cgiargs);
	strcpy(filename, _filename);
	if (stat(_filename, &_sbuf) < 0) {
		requestError(fd, filename, "404", "Not found", "CS537 Server could not find this file");
		return;
	}
	*isStatic = _isStatic;
	//strcpy(fileMode, (char*)_sbuf.st_mode);
	if (_isStatic) {
		if (!(S_ISREG((_sbuf).st_mode)) || !(S_IRUSR & (_sbuf).st_mode)) {
		    requestError(fd, filename, "403", "Forbidden", "CS537 Server could not read this file");
		}
		
		*fileSize = (int)((_sbuf).st_size); 
	} else {
		if (!(S_ISREG((_sbuf).st_mode)) || !(S_IXUSR & (_sbuf).st_mode)) {
	        	*modeErr = 1;
		}
	}
}

void requestHandleSff(int fd, int isStatic, int fileSize, int modeError, char* cgiargs, char* method, char* uri, char* version, char* filename,suseconds_t _statReqArrival, suseconds_t _statReqDispatch, int _age, int _id, int _reqHandld, int _statReq, int _dynReq) {
	printf("%s %s %s\n", method, uri, version);

	statReqArrival = _statReqArrival;
	statReqDispatch = _statReqDispatch;
	age = _age;
	thdId = _id;
	reqHandld = _reqHandld;
	statReq = _statReq;
	dynReq = _dynReq;
	if(isStatic) {
		requestServeStatic(fd, filename, fileSize);
	} else {
		if (modeError) {
		    requestError(fd, filename, "403", "Forbidden", "CS537 Server could not run this CGI program");
		    return;
		}
		requestServeDynamic(fd, filename, cgiargs);
	}

}
