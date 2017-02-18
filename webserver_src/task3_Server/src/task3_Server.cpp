/*
 * task3_Server.cpp
 *
 *  Created on: 15.08.2016
 *      Author: root
 */

/*
 * Notes:
 * Для запуска сервера по порту 80 необходимо(именно этот порт по стандарту занимает web-server), чтобы этот
 * порт был свободен. Обычно его занимает Apache, поэтому сначала его выключаем: /etc/init.d/apache2 stop
 *
 * В параметрах линковщика:
 * -l boost_regex, pthread
 * -L /root/usr/lib, /root/boost_1_46_1/libs
 */

#include "task3_Server.h"

//просто смотрим какой формат у файла, это для запуска скриптов
bool the_end_file_name(char* str, const char* end) {

	int lenS = strlen(str);
		int lenE = strlen(end);
		if(lenS < lenE)
			return false;
		int s = lenS - lenE;
		for(int i = s, j = 0; i < lenS; i++, j++)
		{
			if(str[i] != end[j])
				return false;
		}
		return true;

}

// Заполняем структуру запроса
void get_request_header(char* request, request_header *rh) {

int i = 0, j = 0;

    // Бежим по request, до первого пробела и смотрим GET или POST
    while(request[i] != ' ') {
        rh->request[j] = request[i];
        j++;
        i++;
    }
    rh->request[j] = 0;
    printf("The type of request %s\n", rh->request);
    i++;
    j = 0;

    // Смотрим имя файла
    while(request[i] != ' ') {
        rh->fileName[j] = request[i];
        j++;
        i++;
    }
    rh->fileName[j] = 0;
    rh->lengthFileName = strlen(rh->fileName) + 1;
    i++;

    printf("filename = %s\n", rh->fileName);
    j = 0;

    // Определяем какой протокол
    while(request[i] != '\n') {
        rh->protocol[j]= request[i];
        j++;
        i++;
    }
    rh->protocol[j] = 0;
    i++;
    printf("prot = %s\n", rh->protocol);

    return;
}

// Преобразует response_header в обычную строчку и возвращает указатель на неё
char* get_response(response_header *resp_header) {

	char* res;
	int len_to_string = 0;
	len_to_string = strlen(resp_header->protocol)
	+ strlen(resp_header->status)
	+ strlen("\nContent-Type: ")
	+ strlen(resp_header->contentType)
	+ strlen("\nContent-Length: ")
	+ strlen(resp_header->contentLength)
	+ strlen("\nConnection: ")
	+ strlen(resp_header->connection)
	+ strlen("\n\n")
	+ strlen(resp_header->content);
	len_to_string++;

	res = new char[len_to_string+1];
	res[0] = 0;
	strcat(res, resp_header->protocol);
	strcat(res, " ");
	strcat(res, resp_header->status);
	//strcat(res, "\nDate: ");
	//strcat(res, resp_header->date);
	strcat(res, "\nServer: ");
	//strcat(res, rh.serverName);
	strcat(res, "\nX-Powered-By: ");
	//strcat(res, resp_header->poweredBy);
	//strcat(res, "\nLast-Modified: ");
	//strcat(res, resp_header->lastModified);
	strcat(res, "\nContent-Language: ");
	//strcat(res, resp_header->contentLanguage);
	strcat(res, "\nContent-Type: ");
	strcat(res, resp_header->contentType);
	strcat(res, "\nContent-Length: ");
	strcat(res, resp_header->contentLength);
	strcat(res, "\nConnection: ");
	strcat(res, resp_header->connection);
	strcat(res, "\n\n");
	strcat(res, resp_header->content);

	return res;
}

// Проверяет является ли файл директорией
bool is_directory(char *name) {
	//Функция stat получает информацию о файле или директории,
	//определенном по pathname, и запоминает ее в структуре, на которую
	//указывает buffer.
	struct stat st;
	//st_mode Битовая маска для информации о режиме
	//файла. Бит S_IFDIR устанавливается,
	//если pathname определяет директорий
	return (stat(name, &st) == 0) && ((st.st_mode & S_IFDIR) != 0);

}

int read_file(char *fileName, char **context) {

	int fd = open(fileName, O_RDONLY);
	// файл не найден
	if(fd == -1)
		return NOT_FOUND;

	size_t file_size = lseek(fd, 0, SEEK_END);
	char *mem = (char *)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
	// ошибка открытия
	if(mem == MAP_FAILED)
		return INTERNAL_SERVER_ERROR;

	context[0] = new char[strlen(mem) + 1];
	strcpy(context[0], mem);
	munmap(mem, file_size);
	close(fd);

	return OK;
}

char* get_string(int pip) {

int fd[2];
ssize_t size;
int len = 0;

	if(pipe(fd) < 0)
		 return 0;

	do {
		char buf[MAX_SIZE_RESPONSE];
		size = read(pip, buf, MAX_SIZE_RESPONSE);
		len += size;
		write(fd[1], buf, size);
	} while(size == MAX_SIZE_RESPONSE);
	//char *result = new char[len + 1];
	char *result = (char *)malloc(sizeof(char) * (len + 1));
	read(fd[0], result, (len + 1));
	printf("result:\n%s\n", result);
	result[len] = 0;

	return result;
}

static void sigfunc(int sig_nr) {

	perror("SIGPIPE вызывает завершение программы\n");

	exit(0);

}

int signal_pipe(void) {

	if(signal(SIGPIPE, sigfunc) == SIG_ERR) {
	perror("Невозможно получить сигнал SIGPIPE\n");
	return 1;
	}

	/*Удаляем все сигналы из множества сигналов*/

	sigemptyset(&signal_leer);

	sigemptyset(&signal_new);

	sigaddset(&signal_new, SIGPIPE);

	//Устанавливаем signal_new и сохраняем его теперь маской сигналов будет signal_old
	if(sigprocmask(SIG_UNBLOCK, &signal_new, &signal_old) < 0)
		return 1;

	return 0;
}

// Исполняет указанный скрипт
int execute_script(char *fileName, char **context, const char *script, const char *path) {

int fd[2], status;
char *args[]= {(char *)script, fileName, NULL};

		//если все норм fd[0] – будет занесен файловый дескриптор
		//, соответствующий выходному потоку данных pip’а и позволяющий выполнять только операцию чтения,
		//а во второй элемент массива – fd[1] – будет занесен файловый дескриптор,
		//соответствующий входному потоку данных и позволяющий выполнять только операцию записи.
		if(pipe(fd) < 0)
			 return INTERNAL_SERVER_ERROR;

	    //форкаем новый процесс
	    //родительскому возращает pid ребенка
	    //ребенку возвращает 0
	    //неудача -1
		int pid = fork();
		if(pid == 0) {
			close(fd[0]);
			dup2(fd[1],1);
			dup2(fd[1],2);
			close(fd[1]);
			execve(path, args,NULL) ;
		}
		//ждем завершения скрипта
		while ( (pid=wait(&status)) > 0 ) {

			if(status != 0)
				return NOT_FOUND;
			close(fd[1]);

			context[0] = get_string(fd[0]);
			if(context[0] == 0)
				return INTERNAL_SERVER_ERROR;
		}
		return OK;
}

// Получает список файлов в директории
int get_dir_list(char *fileName, char **context) {

DIR *dp;
int fd[2];
struct dirent *dirp;

	printf("1 lengthfilName: %d fileName:%s\n", strlen(fileName), fileName);
	if((dp = opendir(fileName)) == NULL) {
		printf("2\n");
		return INTERNAL_SERVER_ERROR;
	}
	printf("3\n");
	if(pipe(fd) < 0)
		 return INTERNAL_SERVER_ERROR;

	write(fd[1],"<h3> Listing of directory(",strlen("<h3> Listing of directory("));
	write(fd[1],fileName,strlen(fileName));
	write(fd[1],")</h3>",strlen(")</h3>"));
	write(fd[1],"<table border='3'>",strlen("<table border='3'>"));

	while ((dirp = readdir(dp)) != NULL) {
		if(strcmp(dirp->d_name, ".")==0 || strcmp(dirp->d_name,"..")==0 )
			continue;
		write(fd[1],"<tr>",strlen("<tr>"));
		write(fd[1],"<td>",4);
		write(fd[1],dirp->d_name,strlen(dirp->d_name));
		write(fd[1],"</td>",5);
		write(fd[1],"</tr>",strlen("</tr>"));

	}
	write(fd[1],"</table>", strlen("</table>"));

	*context = get_string(fd[0]);
	if(closedir(dp) != 0){
		printf("Error! Can't close dir.\n");
		return INTERNAL_SERVER_ERROR;
	}
	if(*context == 0)
		return INTERNAL_SERVER_ERROR;
	printf("5\n");
	return OK;
}

// Заполняет структурку ответа
void get_response_header(request_header *rh, response_header *resp_header) {
	printf("1\n");
char *context;
printf("2\n");
char *fullFileName;
size_t lengthFullFileName;
const char* perl = "perl";
const char* uperl = "/usr/bin/perl";
const char* python = "python";
const char* upython = "/usr/bin/python";
const char* php = "php";
const char* uphp = "/usr/bin/php";
int Status;

    strcpy(resp_header->protocol, rh->protocol);
    strcpy(resp_header->contentType, "text/html; charset=windows-1251");
    strcpy(resp_header->connection, "close");
    if(strcmp(rh->request, "GET") == 0) {

    	// Формируем полное имя до файла

    	lengthFullFileName = strlen(rh->fileName) + dir_main.len;
    	fullFileName = (char *)malloc(lengthFullFileName * sizeof(char));

    	strncpy(fullFileName, dir_main.dir, dir_main.len);
    	strncat(fullFileName, rh->fileName, rh->lengthFileName);
    	printf("fullFileName: %s rh->fileName: %s\n", fullFileName, rh->fileName);

    	if(the_end_file_name(rh->fileName, ".pl")) {
    		printf(">>> Perl script!\n");
    		Status = execute_script(fullFileName, &context, perl, uperl);
    	} else if(the_end_file_name(rh->fileName, ".py")) {
    		printf(">>> Python script!\n");
    		Status = execute_script(fullFileName, &context, python, upython);
    	} else if(the_end_file_name(rh->fileName, ".php")) {
    		printf(">>> Php script!\n");
    		Status = execute_script(fullFileName, &context, php, uphp);
    	} else if(is_directory(fullFileName)) {
    		printf(">>>dir\n");
    		Status = get_dir_list(fullFileName, &context);
    	}
    	else {
    		printf(">>>file\n");
    		Status = read_file(fullFileName, &context);
    	}

        if(Status != OK) {
        	strcpy(resp_header->contentLength, "0");
        	strcpy(resp_header->content, "\n");
        	char str[16];
        	sprintf(str, "%d", Status);
        	strcpy(resp_header->status, str);
            return;
        }
        //все ок
        strcpy(resp_header->status, "200 OK");
		char str[16];
        sprintf(str, "%d", strlen(context) + 1);
		strcpy(resp_header->contentLength, str);
		printf("6\n");
		//malloc_lock();
		resp_header->content = new char[strlen(context) + 1];
		//malloc_unlock();
		strcpy(resp_header->content, context);
		free(fullFileName);
		printf("7\n");
		//free(context);
		printf("8\n");

        return;

    }
    //не то
    strcpy(resp_header->status, "500");
	strcpy(resp_header->contentLength, "0");
	strcpy(resp_header->content, "\n");

    return;
}

char* get_response_from_server(char* buf_request, request_header *rh, response_header *resp_header) {

	//заполняем структуру запроса
	get_request_header(buf_request, rh);

	//заполняем структуру ответа
	get_response_header(rh, resp_header);

	//вернём всё в виде строки
	char* buf_response = get_response(resp_header);
	return buf_response;
}

void answer_to_display(char* buf_response, int sock) {

char b[2];

	for(int i = 10; i < 13; i++)
		b[i-10] = buf_response[i];
	int value_status = atoi(b);

	char buf_answer_display[MAX_SIZE];
	if(value_status == NOT_FOUND) {
	   strcpy(buf_answer_display, "404\nNot Found.");
	   printf("%s\n", buf_answer_display);
	   send(sock, buf_answer_display, strlen(buf_answer_display), 0);
	}
	else if(value_status == INTERNAL_SERVER_ERROR) {
	   strcpy(buf_answer_display, "500\nInternal Server Error.");
	   printf("%s\n", buf_answer_display);
	   send(sock, buf_answer_display, strlen(buf_answer_display), 0);
	}

	else
		// посылаем данные в сокет
		send(sock, buf_response, strlen(buf_response), 0);

	printf("%s\n", buf_response);

}

// Функция, в которую передаем функцию, которую надо запустить как отдельный поток, и аргументы этой ф-ции.
int start_thread(int (*fn)(void *), void *i) {

long retval;
void **newstack;

    // создаем стек для нового процесса
    newstack = (void **) malloc(STACKSIZE);
    if (!newstack)
        return -1;
    newstack = (void **) (STACKSIZE + (char *) newstack);
    retval = clone(fn, newstack, (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND ), i);

    if (retval < 0)
        retval = -1;
    return retval;
}

//int child_main(void *i) {
void * child_main(void *i) {

int j = (int)i;
request_header rh;
response_header resp_header;

	// Выделяем память под данные, на которые будет ссылаться структура запроса
	rh.request   = new char[20];
	rh.fileName  = new char[200];
	rh.protocol  = new char[20];

	// Выделяем память под данные, на которые будет ссылаться структура ответа
	resp_header.protocol        = new char[MAX_SIZE];
	resp_header.status          = new char[MAX_SIZE];
	resp_header.contentType     = new char[MAX_SIZE];
	resp_header.contentLength   = new char[MAX_SIZE];
	resp_header.connection      = new char[MAX_SIZE];

	printf("My pid is %d. My index is %d. I am waiting!\n", getpid(), j);
	while(1) {
		pthread_mutex_lock(&signals_array[j]);
		sleep(1);
		printf("Thread: index %d Sock = %d\n", j, client_socks[j]);
		char buf_request[MAX_SIZE_RESPONSE];
		// Получаем запрос
		int size_of_result = recv(client_socks[j], buf_request, MAX_SIZE_RESPONSE, 0);//вернет значение равное, тому сколько передаст
		buf_request[size_of_result] = 0;
		printf("size_of_result = %d Request:\n %s\n", size_of_result, buf_request);
		// Формируем ответ
		char* buf_response = get_response_from_server(buf_request, &rh, &resp_header);

		answer_to_display(buf_response, client_socks[j]);

		if(close(client_socks[j]) != 0)
			printf("Error! Can't close socket %d.", client_socks[j]);
		printf("10\n");
		delete [] resp_header.content;
		printf("11\n");
		//sleep(10);
		pthread_mutex_unlock(&states_array[j]);
	}

	return 0;
}

int main(int argc, char *argv[]) {

int p = 0, result = 0, rc, pid, sock2, status;
pthread_t thread;
struct sockaddr_in addr;
struct sockaddr_in addr2;

	// Работа с аргументами, которые будем передавать
    // Предполагаем что  p - port, d - directory
	while ((result = getopt(argc, argv, "p:d:")) != -1) {
		switch (result) {
			case 'p':
				p = atoi(optarg);
				break;
			case 'd':
				// заполняем структурку dir_name
				// длина полного пути до каталога сервера
				dir_main.len = strlen(optarg) + 1;
				// полный путь до каталога сервера
				dir_main.dir = (char*)malloc(dir_main.len);
				strcpy(dir_main.dir, optarg);
				break;

	    };
	}
	printf("The port is bound to >>> %d\n", p);
	printf("Directory server >>> %s\n\n", dir_main.dir);

	// создаём достаточное кол-во потоков для обработки запросов
	for(int i = 0; i < NUM_OF_PROCESSES; ++i) {
		pthread_mutex_lock(&signals_array[i]);
		pthread_mutex_unlock(&states_array[i]);
		status = pthread_create(&thread, NULL, child_main, (void *)i);
		if (status != 0) {
			printf("main error: can't create thread, status = %d\n", status);
			return 1;
		}
		/*
		pid = start_thread(child_main, (void *)i);
		printf("Index is %d\n", i);
		if (pid < 0) {
			printf("Error! Can't create threads.\n");
			return 1;
		}
		*/
	}

    socklen_t size = sizeof (addr2);
    // создаем сокет
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    printf("Server socket is create!!! (SUCCESS) \n socket = %i\n", sock);
    memset(&addr, 0, sizeof (addr));

    // заполняем структурку addr типа sockaddr_in
    addr.sin_family = AF_INET; // иден. домена
    addr.sin_port = htons(p);  // порт
    addr.sin_addr.s_addr = 0;  // любой

    // связываем сокет с адресом
    if(bind(sock, (struct sockaddr*)&addr, sizeof (struct sockaddr_in)) == -1) {
    	printf("Error number: %d! Can't binding.\n", errno);
    	return 1;
    }
    else
    	printf("bind SUCCESS!\n");
    // установка соединения
    listen(sock, MAX_SIZE);

    // ждем соедениея с сервером
    while(1) {
    	// Создание нового сокета для общения с клиентом
        sock2 = accept(sock, (struct sockaddr*)&addr2, &size);
        // если не создался
        if(sock2 == -1) {
            printf("Error number: %d! Can't create client socket.\n", errno);
            return 1;
        }
        printf("Client socket is create!!! (SUCCESS) \n socket = %i\n", sock2);

        // Опрашиваем каждый процесс свободен ли он.
        int i = 0;
        while(1) {
        	if(i == NUM_OF_PROCESSES)
        		i = 0;
        	if ( (rc = pthread_mutex_trylock(&signals_array[i])) == EBUSY ) {
        		if ( (rc = pthread_mutex_trylock(&states_array[i])) == 0 ) {
        			printf("Server: index %d\n", i);
        			client_socks[i] = sock2;
        			pthread_mutex_unlock(&signals_array[i]);
        			break;
        		}
        	}
        	++i;
        }
    }

    return 0;
}
