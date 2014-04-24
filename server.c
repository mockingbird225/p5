#include "cs537.h"
#include "request.h"
#define MAX 6
// 
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//
char* sAlgo; // 0-FIFO 1-SFF 2-SFF-BS
int numReq; // Not SFF-BS, then 0;
int count;
int bufferSize;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t consumerCV = PTHREAD_COND_INITIALIZER;
pthread_cond_t producerCV = PTHREAD_COND_INITIALIZER;
pthread_cond_t epochCV = PTHREAD_COND_INITIALIZER;

struct List {
	int fd;
	int fileSize;
	int isStatic;
	int modeErr;
	char cgiargs[MAXLINE];
	char method[MAXLINE];
	char uri[MAXLINE];
	char version[MAXLINE];
	char filename[MAXLINE];
	struct List* next;
}*head;

void initializeList() {
	count = 0;
	/*q->bufferSize = numBuffers;
	q->front = -1;
	q->rear = -1;
	q->count = 0;
	q->algo = sAlgo;
	q->numReq = _numReq;
	*/
}

/*void printQueue(Queue* q) {
	int i;
	printf("BufferSize: %d, Algo: %s, NumReq: %d, Front: %d, Rear: %d, Count: %d\n", q->bufferSize, q->algo, q->numReq, q->front, q->rear, q->count);
	printf("Elements in queue\n");
	for(i = 0; i <= q->rear; i++) {
		printf("Elt: %d fileSize: %d, filename : %s\n ", q->qElts[i], q->fileSize[i], q->filename[i]);
	}	
}*/

void enqueueFifo(int x) {
	struct List* temp;
	temp = head;
	struct List* temp1 = (struct List*)malloc(sizeof(struct List));	
	temp1->fd = x;

	if(head == NULL) {
		head = temp1;
		head->next = NULL;
	} else {
		while(temp->next != NULL) {
			temp = temp->next;
		}
		temp1->next = NULL;
		temp->next = temp1;
	}
	count++;
}


void enqueueSff(int connFd, int _isStatic, int _fileSize, int _modeErr, char* _cgiargs, char*_method, char* _uri, char* _version, char* _filename) {
	struct List* temp = head;
	int c = 0, isInsert = 0;
	struct List* temp1 = (struct List*)malloc(sizeof(struct List));
	temp1->fd = connFd;
	temp1->isStatic = _isStatic;
	temp1->fileSize = _fileSize;
	temp1->modeErr = _modeErr;
	strcpy(temp1->cgiargs, _cgiargs);
	strcpy(temp1->method, _method);
	strcpy(temp1->uri, _uri);
	strcpy(temp1->version, _version);
	strcpy(temp1->filename, _filename);
	if(head == NULL) {
		head = temp1;
		head->next = NULL;
		count++;
	} else {
		struct List* prev = NULL;
		while(temp != NULL && !isInsert) {
			if(temp1->fileSize < temp->fileSize) {
				if(prev) {
					temp1->next = temp;
					prev->next = temp1;
				} else {
					temp1->next = head;
					head = temp1;
				}
				isInsert = 1;
				count++;
				temp = temp->next;
			} else {
				prev = temp;
				temp = temp->next;	
				c++;
			}
		}
		if(!isInsert && c == count) {
			prev->next = temp1;
			temp1->next = NULL;
			count++;
		}
	}
}

int isListEmpty() {
	return (count == 0);
}

int isListFull() {
	return (count == bufferSize);
}

int dequeueFifo() {
	int x;
	if(isListEmpty()) {
		printf("List empty\n");
	} else {
		x = head->fd;
		head = head->next;
		count--;
		return x;
	}
	return -1;
} 

int dequeueSff(int* _isStatic, int* _fileSize, int* _modeErr, char* _cgiargs, char* _method, char* _uri, char* _version, char* _filename) {
	int x;
	if(isListEmpty()) {
		printf("List Empty\n");
	} else {
		x = head->fd;
		*_isStatic = head->isStatic;
		*_fileSize = head->fileSize;
		*_modeErr = head->modeErr;
		strcpy(_cgiargs, head->cgiargs);
		strcpy(_method, head->method);
		strcpy(_uri, head->uri);
		strcpy(_version, head->version);
		strcpy(_filename, head->filename);
		count--;
		head = head->next;
		return x;
	}
	return -1;
}

// CS537: TODO make it parse the new arguments too
int getargs(int *port, int *numThreads, int* numBuffers, char* sAlgo, int argc, char* argv[]) {
	int numReq = -1;
	if(argc < 5 || argc > 6) {
		fprintf(stderr, "Usage : %s [port] [threads] [buffers] [schedalg] [N (for SFF-BS only)]\n", argv[0]);
		exit(1);
	}
	*port = atoi(argv[1]);
	*numThreads = atoi(argv[2]);
	if(*numThreads < 0) {
		fprintf(stderr, "[threads] should be a positive integer\n");
		exit(1);
	}
	*numBuffers = atoi(argv[3]);
	if(*numBuffers < 0) {
		fprintf(stderr, "[buffers] should be a positive integer\n");
		exit(1);
	}
	if((strcmp(argv[4], "FIFO") && strcmp(argv[4], "SFF") && strcmp(argv[4], "SFF-BS"))) {
		fprintf(stderr, "[schedalg] should be one of FIFO, SFF or SFF-BS\n");
		exit(1);
	} else {
		strcpy(sAlgo, argv[4]);
		if(!strcmp(sAlgo, "SFF-BS")) {
			if(argc == 5) {
				fprintf(stderr, "Usage : ./server [port] [threads] [buffers] [schedalg] [N (for SFF-BS only)]\n");
				exit(1);
			}
			numReq = atoi(argv[5]);
			if(numReq < 0) {
				fprintf(stderr, "Usage: [N] should be a positive integer\n");
				exit(1);
			}
		}
	}
	return numReq;
}
/*void getargs(int *port, int argc, char *argv[])
{
	if (argc != 2) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
	}
	*port = atoi(argv[1]);
}*/

void fifo() {
	while(1) {
		int req;
		int qFullFlag = 0;
		pthread_mutex_lock(&lock);
		while(isListEmpty()) {
			pthread_cond_wait(&consumerCV, &lock);	
		}		
		if(isListFull()) {
			qFullFlag = 1;
		}
		req = dequeueFifo();
		if(qFullFlag) {
			pthread_cond_signal(&producerCV);
		}
		pthread_mutex_unlock(&lock);
		requestHandleFifo(req);
		Close(req);
	}
}

void sff() {
	while(1) {
		int req;
		int _isStatic, _fileSize, _modeErr;
		char _cgiargs[MAXLINE], _method[MAXLINE], _uri[MAXLINE], _version[MAXLINE], _filename[MAXLINE];
		int qFullFlag = 0;
		pthread_mutex_lock(&lock);
		while(isListEmpty()) {
			pthread_cond_wait(&consumerCV, &lock);
		}
		if(isListFull()) {
			qFullFlag = 1;
		}
		req = dequeueSff(&(_isStatic), &(_fileSize), &(_modeErr), _cgiargs, _method, _uri, _version, _filename);
		if(qFullFlag) {
			pthread_cond_signal(&producerCV);	
		}
		pthread_mutex_unlock(&lock);
		requestHandleSff(req, _isStatic, _fileSize, _modeErr, _cgiargs, _method, _uri, _version, _filename);
		Close(req);

	}
}

void sffBs() {
	while(1) {
		int req;
		int _isStatic, _fileSize, _modeErr;
		char _cgiargs[MAXLINE], _method[MAXLINE], _uri[MAXLINE], _version[MAXLINE], _filename[MAXLINE];

		pthread_mutex_lock(&lock);
		if(isListEmpty()) {
			pthread_cond_wait(&consumerCV, &lock);
		}
		req = dequeueSff(&(_isStatic), &(_fileSize), &(_modeErr), _cgiargs, _method, _uri, _version, _filename);
		if(count == 0) {
			pthread_cond_signal(&epochCV);
		}	
		pthread_cond_signal(&producerCV);
		pthread_mutex_unlock(&lock);
		requestHandleSff(req, _isStatic, _fileSize, _modeErr, _cgiargs, _method, _uri, _version, _filename);
		Close(req);
	}	
}

void* handleRequest() {
	//struct List* head = (struct List*)_head;
	if(!strcmp(sAlgo, "FIFO")) {
		// First in first out implementation
		fifo();
	} else if(!strcmp(sAlgo, "SFF")) {
		//printf("Thread\n");
		sff();
	} 
	else if(!strcmp(sAlgo, "SFF-BS")) {
		//Smallest File First implementation
		sffBs();
	} /*`else if(!strcmp(q->algo, "SFF-BS")) {
		// Smallest File first with bounded starvation implementation 
		sffbs(q);
	}*/
	return 0;
}

void processConn(int connFd) {
	int qEmptyFlag = 0, _isStatic, _fileSize, modeErr = 0;
	char _cgiargs[MAXLINE], _method[MAXLINE], _uri[MAXLINE], _version[MAXLINE], _filename[MAXLINE]; 
	 
	pthread_mutex_lock(&lock);
	if(isListFull()) {
		printf("List is full\n");
		pthread_cond_wait(&producerCV, &lock);
	}	
	if(isListEmpty()) {
		qEmptyFlag = 1;
	}
	if(!strcmp(sAlgo, "FIFO")) {
		enqueueFifo(connFd);
	} else if(!strcmp(sAlgo, "SFF")) {
		findReqSize(connFd, &(_isStatic), &(_fileSize),  &modeErr, _cgiargs, _method, _uri, _version, _filename);
		enqueueSff(connFd, _isStatic, _fileSize, modeErr, _cgiargs, _method, _uri, _version, _filename);
	} else if(!strcmp(sAlgo, "SFF-BS")) {
		if(numReq <= 0) {
			//fprintf(stderr, "Error in epoch\n");
		} else if(numReq >= bufferSize){
			findReqSize(connFd, &(_isStatic), &(_fileSize),  &modeErr, _cgiargs, _method, _uri, _version, _filename);
			enqueueSff(connFd, _isStatic, _fileSize, modeErr, _cgiargs, _method, _uri, _version, _filename);
		} else {
			if(count >= numReq) {
				//printf("Greater than epoch\n");	
				pthread_cond_wait(&epochCV, &lock);
			}
			findReqSize(connFd, &(_isStatic), &(_fileSize),  &modeErr, _cgiargs, _method, _uri, _version, _filename);
			enqueueSff(connFd, _isStatic, _fileSize, modeErr, _cgiargs, _method, _uri, _version, _filename);
		}
	}
	if(qEmptyFlag) {
		pthread_cond_signal(&consumerCV);
	}
	pthread_mutex_unlock(&lock);
}

void printList() {
	struct List* temp = head;
	printf("-------------List------------\n");
	while(temp != NULL) {
		printf("Ele: %d\n", temp->fd);
		printf("File size: %d\n", temp->fileSize);
		temp = temp->next;
	}	
	printf("----------List complete----------\n");
}

int main(int argc, char *argv[])
{
	head = NULL;
	int listenfd, connfd, clientlen;
	int port, numThreads, temp;
	int rc;
	int _numReq = -1;
	char algo[MAX];
	int i;
	struct sockaddr_in clientaddr;

	temp = getargs(&port, &numThreads, &bufferSize, algo, argc, argv);
	if(temp >= 0) {
		_numReq = temp;
	}
	pthread_t threads[numThreads];
	sAlgo = malloc(sizeof(char)*strlen(algo));
	strcpy(sAlgo, algo);
	numReq = _numReq;
	initializeList();
	for(i = 0; i < numThreads; i++) {
		rc = pthread_create(&threads[i], NULL, handleRequest, NULL);
		if(rc != 0) {
			//fprintf(stderr, "Thread creation failed\n");
		}
	}
	listenfd = Open_listenfd(port);
	while (1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
		processConn(connfd);
		// -------------------Printing the queue-------------
		//printList();
	}
	return 0;
}


    


 
