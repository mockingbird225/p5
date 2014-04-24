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

struct List {
	int fd;
	int bufferSize;
	int fileSize;
	int isStatic;
	char* cgiargs;
	char* method;
	char* uri;
	char* version;
	char* filename;
	struct List* next;
}*head;

void initializeList() {
	count = 0;
	/*q->bufferSize = numBuffers;
	q->qElts = malloc(sizeof(int)*(q->bufferSize));
	q->isStatic = malloc(sizeof(int)*(q->bufferSize));
	q->fileSize = malloc(sizeof(int)*(q->bufferSize));
	q->cgiargs = malloc(sizeof(char*)*(q->bufferSize));
	q->method= malloc(sizeof(char*)*(q->bufferSize));
	q->uri = malloc(sizeof(char*)*(q->bufferSize));
	q->version = malloc(sizeof(char*)*(q->bufferSize));
	q->filename= malloc(sizeof(char*)*(q->bufferSize));
	q->fileMode = malloc(sizeof(mode_t)*(q->bufferSize));
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

int isListEmpty() {
	return (count == 0);
}

int isListFull() {
	return (count == bufferSize-1);
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

/*void sff(struct List* q) {
	int req;
	int qFullFlag = 0;
	pthread_mutex_lock(&lock);
	while(isQueueEmpty(q)) {
		pthread_cond_wait(&consumerCV, &lock);	
	}		
	if(isQueueFull(q)) {
		qFullFlag = 1;
	}
	req = dequeue(q);
		if(qFullFlag) {
		pthread_cond_signal(&producerCV);
	}
	pthread_mutex_unlock(&lock);
	requestHandle(req);
	Close(req);

}

void sffbs(Queue* q) {
	printf("SFF-BS with %d requests\n", q->numReq);
}*/

void* handleRequest() {
	//struct List* head = (struct List*)_head;
	if(!strcmp(sAlgo, "FIFO")) {
		// First in first out implementation
		fifo();
	} 
	/*else if(!strcmp(q->algo, "SFF")) {
		//Smallest File First implementation
		sff(q);
	} else if(!strcmp(q->algo, "SFF-BS")) {
		// Smallest File first with bounded starvation implementation 
		sffbs(q);
	}*/
	return 0;
}

void processConn(int connFd) {
	int qEmptyFlag = 0, _isStatic, _fileSize;
	char* _cgiargs, *_method, *_uri, *_version, *_filename, *fileMode;
	int fileSize;
	 
	pthread_mutex_lock(&lock);
	if(isListFull()) {
		pthread_cond_wait(&producerCV, &lock);
	}	
	if(isListEmpty()) {
		qEmptyFlag = 1;
	}
	if(!strcmp(sAlgo, "FIFO")) {
		enqueueFifo(connFd);
	}
	if(qEmptyFlag) {
		pthread_cond_signal(&consumerCV);
	}
	pthread_mutex_unlock(&lock);
}

void printList() {
	struct List* temp = head;
	while(temp != NULL) {
		printf("Test\n");
		printf("Ele: %d\n", temp->fd);
		temp = temp->next;
	}	
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
			fprintf(stderr, "Thread creation failed\n");
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


    


 
