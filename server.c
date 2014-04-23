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
int numBuffers;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t consumerCV = PTHREAD_COND_INITIALIZER;
pthread_cond_t producerCV = PTHREAD_COND_INITIALIZER;


typedef struct Queue_Req Queue; 
struct Queue_Req {
	int *qElts;
	int bufferSize;
	int *fileSize;
	char* algo; // 0-FIFO 1-SFF 2-SFF-BS
	int numReq; // Not SFF-BS, then 0;
	int front;
	int rear;
	int count;
	int isStatic;
	struct stat sbuf;
};

void initializeQueue(Queue* q, char* sAlgo, int _numReq) {
	q->bufferSize = numBuffers;
	q->qElts = malloc(sizeof(int)*(q->bufferSize));
	q->front = -1;
	q->rear = -1;
	q->count = 0;
	q->fileSize = malloc(sizeof(int)*(q->bufferSize));
	q->algo = sAlgo;
	q->numReq = _numReq;
}

void printQueue(Queue* q) {
	int i;
	printf("BufferSize: %d, Algo: %s, NumReq: %d, Front: %d, Rear: %d, Count: %d\n", q->bufferSize, q->algo, q->numReq, q->front, q->rear, q->count);
	printf("Elements in queue\n");
	for(i = 0; i <= q->rear; i++) {
		printf("%d ", q->qElts[i]);
	}	
}

void enqueue(Queue* q, int x, int _fileSize) {
	if(q->count >= q->bufferSize) {
		printf("Queue overflow\n");
	} else {
		q->rear = q->rear + 1;
		q->qElts[(q->rear % q->bufferSize)] = x;
		q->fileSize[(q->rear % q->bufferSize)] = _fileSize;
		q->count++;
	}
}

int isQueueEmpty(Queue *q) {
	return (q->front == q->rear);
}

int isQueueFull(Queue* q) {
	return ((q->rear - q->bufferSize) == q->front);
}

int dequeue(Queue* q) {
	int x;
	int fSize;
	if(isQueueEmpty(q)) {
		printf("Queue empty\n");
	} else {
		q->front++;
		x = q->qElts[(q->front % q->bufferSize)];
		fSize = q->fileSize[(q->front % q->bufferSize)];
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
		//*sAlgo = argv[4];
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

void fifo(Queue* q) {
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

void sff(Queue* q) {
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
}

void* handleRequest(void* queue) {
	Queue* q = (Queue*)queue;
	if(!strcmp(q->algo, "FIFO")) {
		// First in first out implementation
		fifo(q);
	} else if(!strcmp(q->algo, "SFF")) {
		//Smallest File First implementation
		sff(q);
	} else if(!strcmp(q->algo, "SFF-BS")) {
		// Smallest File first with bounded starvation implementation 
		sffbs(q);
	}
	return 0;
}

void processConn(int connFd, Queue* q, char* algo) {
	int qEmptyFlag = 0;
	int fileSize;
	if(!strcmp(algo, "FIFO")) {
		fileSize = -1;
	} else {
		printf("Finding file size\n");
		fileSize = findReqSize(connFd);	
	}
	pthread_mutex_lock(&lock);
	if(isQueueFull(q)) {
		pthread_cond_wait(&producerCV, &lock);
	}	
	if(isQueueEmpty(q)) {
		qEmptyFlag = 1;
	}
	printf("File Size: %d\n", fileSize);
	enqueue(q, connFd, fileSize);
	if(qEmptyFlag) {
		pthread_cond_signal(&consumerCV);
	}
	pthread_mutex_unlock(&lock);
}

int main(int argc, char *argv[])
{
	Queue q;
	int listenfd, connfd, clientlen;
	int port, numThreads, numReq, temp;
	int rc;
	char sAlgo[MAX];
	int i;
	struct sockaddr_in clientaddr;

	temp = getargs(&port, &numThreads, &numBuffers, sAlgo, argc, argv);
	if(temp >= 0) {
		numReq = temp;
	}
	pthread_t threads[numThreads];

	initializeQueue(&q, sAlgo, numReq);
	for(i = 0; i < numThreads; i++) {
		rc = pthread_create(&threads[i], NULL, handleRequest, (void*)&q);
		if(rc != 0) {
			fprintf(stderr, "Thread creation failed\n");
		}
	}
	listenfd = Open_listenfd(port);
	while (1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
		processConn(connfd, &q, sAlgo);
		// -------------------Printing the queue-------------
		//printQueue(&q);
	}
	return 0;
}


    


 
