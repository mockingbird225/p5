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

// CS537: TODO make it parse the new arguments too
int getargs(int *port, int *numThreads, int* numBuffers, char* sAlgo, int argc, char* argv[]) {
	int numReq = -1;
	if(argc < 5 || argc > 6) {
		fprintf(stderr, "Usage : %s [port] [threads] [buffers] [schedalg] [N (for SFF-BS only)]\n");
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
				fprintf(stderr, "Usage : %s [port] [threads] [buffers] [schedalg] [N (for SFF-BS only)]\n");
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


int main(int argc, char *argv[])
{
	int listenfd, connfd, clientlen;
	int port, numThreads, numBuffers, numReq, temp;
	char sAlgo[MAX];
	struct sockaddr_in clientaddr;

	//getargs(&port, argc, argv);
	temp = getargs(&port, &numThreads, &numBuffers, sAlgo, argc, argv);
	if(temp >= 0) {
		numReq = temp;
	}
	/* ------------Test----------------
	printf("Port: %d, numThreads : %d, numBuffers : %d, Algo : %s", port, numThreads, numBuffers, sAlgo);
	*/ 
	// CS537: TODO create some worker threads using pthread_create ...
	//

	/*listenfd = Open_listenfd(port);
	while (1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

		// 
		// CS537: In general, don't handle the request in the main thread.
		// TODO In multi-threading mode, the main thread needs to
		// save the relevant info in a buffer and have one of the worker threads
		// do the work.
		// 
		requestHandle(connfd);

		Close(connfd);
	}*/

}


    


 
