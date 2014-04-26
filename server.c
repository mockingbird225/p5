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
int thdCount;
int bufferSize;
int numThreads;
int tempBuffer;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t thdLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t consumerCV = PTHREAD_COND_INITIALIZER;
pthread_cond_t producerCV = PTHREAD_COND_INITIALIZER;
pthread_cond_t epochCV = PTHREAD_COND_INITIALIZER;

struct timeval tv;


/*typedef struct __statistics{
	time_t arrival;
}statistics;
statistics *stats;*/

suseconds_t statReqDispatch;
suseconds_t statReqRead;
suseconds_t statReqComplete;

struct List {
	int fd;
	int fileSize;
	int isStatic;
	int modeErr;
	int age;
	char cgiargs[MAXLINE];
	char method[MAXLINE];
	char uri[MAXLINE];
	char version[MAXLINE];
	char filename[MAXLINE];
	suseconds_t statReqArrival;

	struct List* next;
}*head;

struct ThreadList {
	int id;
	int reqHandld;
	int statReqHandled;
	int dynReqHandled;
	struct ThreadList *next
}*headThd;

void initializeList() {
	count = 0;
	thdCount = -1;
	tempBuffer = numReq;
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


void enqueueFifoNew(int connFd, int _isStatic, int _fileSize, int _modeErr, char* _cgiargs, char* _method, char* _uri, char* _version, char* _filename, suseconds_t _statReqArrival) {
	struct List* temp;
	temp = head;
	struct List* temp1 = (struct List*)malloc(sizeof(struct List));	
	temp1->fd = connFd;
	temp1->isStatic = _isStatic;
	temp1->fileSize = _fileSize;
	temp1->modeErr = _modeErr;
	temp1->age = 0;
	strcpy(temp1->cgiargs, _cgiargs);
	strcpy(temp1->method, _method);
	strcpy(temp1->uri, _uri);
	strcpy(temp1->version, _version);
	strcpy(temp1->filename, _filename);
	temp1->statReqArrival = _statReqArrival;

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

/*void enqueueFifo(int x, time_t _statReqArrival) {
	struct List* temp;
	temp = head;
	struct List* temp1 = (struct List*)malloc(sizeof(struct List));	
	temp1->fd = x;
	temp1->statReqArrival = _statReqArrival;
	temp1->age = 0;

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
}*/

void insertThdList(int thdCount) {
	struct ThreadList* temp;
	temp = headThd;
	struct ThreadList* temp1 = (struct ThreadList*)malloc(sizeof(struct ThreadList));
	temp1->id = thdCount;
	temp1->reqHandld = 0;
	temp1->statReqHandled = 0;
	temp1->dynReqHandled = 0;
	temp1->next = NULL;

	if(headThd == NULL) {
		headThd = temp1;
	} else {
		while(temp->next != NULL) {
			temp = temp->next;
		}
		temp->next = temp1;
	}
}

void updateAge(struct List* temp) {
	temp = temp-> next;
	while(temp != NULL) {
		temp->age++;
		temp = temp->next;
	}
}

void enqueueSff(int connFd, int _isStatic, int _fileSize, int _modeErr, char* _cgiargs, char*_method, char* _uri, char* _version, char* _filename, suseconds_t _statReqArrival) {
	struct List* temp = head;
	int c = 0, isInsert = 0;
	struct List* temp1 = (struct List*)malloc(sizeof(struct List));
	temp1->fd = connFd;
	temp1->isStatic = _isStatic;
	temp1->fileSize = _fileSize;
	temp1->modeErr = _modeErr;
	temp1->age = 0;
	strcpy(temp1->cgiargs, _cgiargs);
	strcpy(temp1->method, _method);
	strcpy(temp1->uri, _uri);
	strcpy(temp1->version, _version);
	strcpy(temp1->filename, _filename);
	temp1->statReqArrival = _statReqArrival;
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
				updateAge(temp1);
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

/*int dequeueFifo(time_t* _statReqArrival, int* _age) {
	int x;
	if(isListEmpty()) {
		printf("List empty\n");
	} else {
		x = head->fd;
		_statReqArrival = head->statReqArrival;
		_age = head->age;
		head = head->next;
		count--;
		return x;
	}
	return -1;
} */


void dequeueThdList(int thdCount, int* _reqHandld, int* _statReq, int* _dynReq) {
	struct ThreadList* temp = headThd;
	while(temp != NULL) {
		if(temp->id == thdCount) {
			*_reqHandld = temp->reqHandld;
			*_statReq = temp->statReqHandled;
			*_dynReq = temp->dynReqHandled;
		}
		temp = temp->next;
	}
}


int dequeueSff(int* _isStatic, int* _fileSize, int* _modeErr, char* _cgiargs, char* _method, char* _uri, char* _version, char* _filename, suseconds_t* _statReqArrival, int* _age) {
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
		*_statReqArrival = head->statReqArrival;
		*_age = head->age;
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


void updateThdListFifo(int _id) {
	struct ThreadList* temp = headThd;
	while(temp != NULL) {
		if(temp->id == _id) {
			temp->reqHandld++;	
		}
		temp = temp->next;
	}
}

void updateThdListSff(int _id, int _isStatic) {
	struct ThreadList* temp = headThd;
	while(temp != NULL) {
		if(temp->id == _id) {
			temp->reqHandld++;
			if(_isStatic) {
				temp->statReqHandled++;
			} else {
				temp->dynReqHandled++;
			}
		}
		temp = temp->next;
	}
}

void updateAgeFifo() {
	struct List* temp = head;
	while(temp != NULL) {
		temp->age++;
		temp = temp->next;
	}
}

/*void fifo(int thdId) {
	time_t _statReqArrival, statReqPick;
	int age;
	while(1) {
		int req;
		int _isStatic, _fileSize, _modeErr;
		char _cgiargs[MAXLINE], _method[MAXLINE], _uri[MAXLINE], _version[MAXLINE], _filename[MAXLINE];
		int staticReq, dynReq, reqHandld;

		int qFullFlag = 0;
		pthread_mutex_lock(&lock);
		while(isListEmpty()) {
			pthread_cond_wait(&consumerCV, &lock);	
		}		
		if(isListFull()) {
			qFullFlag = 1;
		}
		//req = dequeueFifo(&(_statReqArrival), &(_age));
		req = dequeueSff(&(_isStatic), &(_fileSize), &(_modeErr), _cgiargs, _method, _uri, _version, _filename, &(_statReqArrival), &(_age));
		//updateThdListFifo(_thdId);
		updateThdListSff(thdCount, _isStatic);
		gettimeofday(&tv, NULL);
		statReqPick = (tv.tv_sec)/1000;
		statReqDispatch = statReqPick - _statReqArrival;	
		if(qFullFlag) {
			pthread_cond_signal(&producerCV);
		}
		pthread_mutex_unlock(&lock);
		//requestHandleFifo(req, _statReqArrival, statReqDispatch, _age);
		requestHandleSff(req, _isStatic, _fileSize, _modeErr, _cgiargs, _method, _uri, _version, _filename, _statReqArrival, statReqDispatch, _age);
		Close(req);
	}
}*/

/*void sffBs(int thdCount) {
	time_t _statReqArrival, statReqDispatch;
	int _age;
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
		req = dequeueSff(&(_isStatic), &(_fileSize), &(_modeErr), _cgiargs, _method, _uri, _version, _filename, &(_statReqArrival), &(_age));
		updateThdListSff(thdCount, _isStatic);
		gettimeofday(&tv, NULL);
		statReqPick = (tv.tv_sec)/1000;
		statReqDispatch = statReqPick - _statReqArrival;	
		if(qFullFlag) {
			pthread_cond_signal(&producerCV);	
		}
		pthread_mutex_unlock(&lock);
		requestHandleSff(req, _isStatic, _fileSize, _modeErr, _cgiargs, _method, _uri, _version, _filename, _statReqArrival, statReqDispatch, _age);
		Close(req);

	}
}*/

void sff(int thdCount) {
	suseconds_t _statReqArrival, statReqDispatch, statReqPick;
	int _age;
	while(1) {
		int req;
		int _isStatic, _fileSize, _modeErr;
		char _cgiargs[MAXLINE], _method[MAXLINE], _uri[MAXLINE], _version[MAXLINE], _filename[MAXLINE];
		int staticReq, dynReq, reqHandld;

		pthread_mutex_lock(&lock);
		if(isListEmpty()) {
			pthread_cond_wait(&consumerCV, &lock);
		}
		req = dequeueSff(&(_isStatic), &(_fileSize), &(_modeErr), _cgiargs, _method, _uri, _version, _filename, &(_statReqArrival), &(_age));
		if(!strcmp(sAlgo, "FIFO")) {
			updateAgeFifo();
		}
		updateThdListSff(thdCount, _isStatic);
		dequeueThdList(thdCount, &reqHandld, &staticReq, &dynReq);
		gettimeofday(&tv, NULL);
		statReqPick = (tv.tv_usec)*1000;
		statReqDispatch = statReqPick - _statReqArrival;	
		if(count == 0) {
			pthread_cond_signal(&epochCV);
		}	
		pthread_cond_signal(&producerCV);
		pthread_mutex_unlock(&lock);
		requestHandleSff(req, _isStatic, _fileSize, _modeErr, _cgiargs, _method, _uri, _version, _filename, _statReqArrival, statReqDispatch, _age, thdCount, reqHandld, staticReq, dynReq);
		Close(req);
	}	
}

void* handleRequest() {
	pthread_mutex_lock(&thdLock);
	thdCount++;
	if(thdCount > bufferSize) {
		fprintf(stderr, "Something wrong! Number of threads exceeds buffersize\n");
		exit(1);
	}
	insertThdList(thdCount);
	pthread_mutex_unlock(&thdLock);
	//struct List* head = (struct List*)_head;
	if(!strcmp(sAlgo, "FIFO")) {
		// First in first out implementation
		sff(thdCount);
	} else if(!strcmp(sAlgo, "SFF")) {
		//printf("Thread\n");
		sff(thdCount);
	} 
	else if(!strcmp(sAlgo, "SFF-BS")) {
		//Smallest File First implementation
		sff(thdCount);
	} /*`else if(!strcmp(q->algo, "SFF-BS")) {
		// Smallest File first with bounded starvation implementation 
		sffbs(q);
	}*/
	return 0;
}

void processConn(int connFd, suseconds_t _statReqArrival) {
	int i;
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
		findReqSize(connFd, &(_isStatic), &(_fileSize),  &modeErr, _cgiargs, _method, _uri, _version, _filename);
		//enqueueFifo(connFd, _statReqArrival);
		enqueueFifoNew(connFd, _isStatic, _fileSize, modeErr, _cgiargs, _method, _uri, _version, _filename, _statReqArrival);
	} else if(!strcmp(sAlgo, "SFF")) {
		findReqSize(connFd, &(_isStatic), &(_fileSize),  &modeErr, _cgiargs, _method, _uri, _version, _filename);
		enqueueSff(connFd, _isStatic, _fileSize, modeErr, _cgiargs, _method, _uri, _version, _filename, _statReqArrival);
	} else if(!strcmp(sAlgo, "SFF-BS")) {
		if(numReq <= 0) {
			fprintf(stderr, "Epoch size should be greater than zero\n");
			exit(1);
		} else if(numReq >= bufferSize){
			findReqSize(connFd, &(_isStatic), &(_fileSize),  &modeErr, _cgiargs, _method, _uri, _version, _filename);
			enqueueSff(connFd, _isStatic, _fileSize, modeErr, _cgiargs, _method, _uri, _version, _filename, _statReqArrival);
		} else {
			tempBuffer--;
			if(tempBuffer < 0) {
				//printf("Greater than epoch\n");	
				pthread_cond_wait(&epochCV, &lock);
			}
			findReqSize(connFd, &(_isStatic), &(_fileSize),  &modeErr, _cgiargs, _method, _uri, _version, _filename);
			enqueueSff(connFd, _isStatic, _fileSize, modeErr, _cgiargs, _method, _uri, _version, _filename, _statReqArrival);
		}
	}
	if(!strcmp(sAlgo, "SFF-BS")) {
		if(tempBuffer > 0) {
			pthread_cond_signal(&consumerCV);
		} else if(tempBuffer <= 0) {
			pthread_cond_signal(&consumerCV);
			tempBuffer = numReq;
		}
	} else {
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
	headThd = NULL;
	int listenfd, connfd, clientlen;
	int port, _numThreads, temp;
	int rc;
	int _numReq = -1;
	char algo[MAX];
	int i;
	struct sockaddr_in clientaddr;
	suseconds_t statReqArrival;
	temp = getargs(&port, &_numThreads, &bufferSize, algo, argc, argv);
	numThreads = _numThreads;
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
		gettimeofday(&tv, NULL);
		statReqArrival = (tv.tv_usec)*1000;
		printf("Time: %f\n",(float)statReqArrival);
		processConn(connfd, statReqArrival);
		// -------------------Printing the queue-------------
		//printList();
	}
	return 0;
} 
