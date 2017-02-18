/*
 * task3_Server.h
 *
 *  Created on: 15.08.2016
 *      Author: root
 */

#ifndef TASK3_SERVER_H_
#define TASK3_SERVER_H_
	#include <pthread.h>
	#include <netinet/in.h>
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <sys/mman.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <sys/wait.h>
	#include <dirent.h>
	#include <signal.h>
	#include <sched.h>
	#include <semaphore.h>
	#include <errno.h>


	using namespace std;

	// Константы
	#define MAX_SIZE 100
	#define MAX_SIZE_RESPONSE 1024
	#define STACKSIZE 16384
	#define NUM_OF_PROCESSES 3

	static volatile sig_atomic_t sflag;
	static sigset_t signal_new, signal_old, signal_leer;
	static pthread_mutex_t signals_array[NUM_OF_PROCESSES] = PTHREAD_MUTEX_INITIALIZER;
	static pthread_mutex_t states_array[NUM_OF_PROCESSES] = PTHREAD_MUTEX_INITIALIZER;
	int client_socks[NUM_OF_PROCESSES];

	// Коды ответа сервера
	#define CONTINUE 						100
	#define SWITCHING_PROTOCOLS 			101
	#define PROCESSING 						102

	#define OK 								200
	#define CREATED 						201
	#define ACCEPTED 						202
	#define NON_AUTHORITATIVE_INFORMATION  	203
	#define NO_CONTENT 						204
	#define RESET_CONTENT 					205
	#define PARTIAL_CONTENT 				206
	#define MULTI_STATUS 					207
	#define IM_USED							208

	#define MULTIPLE_CHOICES 				300
	#define MOVED_PERMANENTLY 				301
	#define MOVED_TEMPORARILY 				302
	#define SEE_OTHER 						303
	#define NOT_MODIFIED					304
	#define USE_PROXY						305
	#define REDSERVED 						306
	#define TEMPORARY_REDIRECT				307

	#define BAD_REQUEST						400
	#define NOT_FOUND 						404

	#define INTERNAL_SERVER_ERROR 			500
	#define NOT_IMPLEMENTED 				501

	struct _dir_name {
		char* dir;
		size_t len;
	} typedef dir_name;;

	struct _response_header {
		char* protocol;
		char* status;
		//char* date;
		//char* serverName;
		//char* contentLanguage;
		char* content;
		char* contentType;
		char* contentLength;
		char* connection;
	} typedef response_header;

	struct _request_header {
		char* request;
		char* fileName;
		size_t lengthFileName;
		char* protocol;

	} typedef request_header;

	// глобальная переменная, в которой будет хранится полный путь каталога сервера
	dir_name dir_main;

#endif /* TASK3_SERVER_H_ */
