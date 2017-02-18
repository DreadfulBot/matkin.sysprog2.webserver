/*
 * для успешной линковки необходимо подключить библиотеку pthread
 * ПКМ по проекту -> Properties -> вкладка Tool Settings -> GCC C++ Linker -> Libraries -> Add -> pthread
 * тестить через неткад - printf "GET / HTTP/1.1\r\n" | nc -n 127.0.0.1 80
 */

#include "task3_Server.h"


/*
 * возвращает true если *str оканчивается на
 * *end (проверяем расширение файла)
 */
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

// заполняем структуру запроса
// возвращает 0 если запрос корректен, иначе -1
int get_request_header(char* request, request_header *rh) {

	int i = 0, j = 0;
	char* buffer, *ptrReq;
	buffer = (char*)calloc(MAX_RQ_CHUNK_SIZE, sizeof(char));

	//базовая валидация заголовка (3 части, разделенных пробелами и оканч. на \r\n
	if((ptrReq = strstr(request, "\r\n")) != NULL)
	{
		int matchCount = 0;
		for(int i=0; i < strlen(request) - strlen(ptrReq); i++)
		{
			if(request[i] == ' ')
				matchCount++;
		}
		if(matchCount != 2)
		{
			free(buffer);
			return -1;
		}

	}
	else
	{
		free(buffer);
		return -1;
	}

	// получаем тип запроса (GET/POST/ETC)...
	strncpy(buffer, request, 5);
	if(strstr(buffer, "GET") == NULL && strstr(buffer, "POST") == NULL)
	{
		free(buffer);
		return -1;
	}

	buffer[5] = 0;
	memset(rh->request, 0, strlen(rh->request));
	strncpy(rh->request, buffer, strlen(buffer)-strlen(strchr(buffer, ' ')));
	printf("request type: %s\n", rh->request);
	memset(buffer, 0, sizeof(char)*MAX_RQ_CHUNK_SIZE);

	// получаем имя запрошенного файла
	ptrReq = request + strlen(rh->request) + 1;
	strncpy(buffer, ptrReq, strlen(ptrReq) - strlen(strchr(ptrReq, ' ')) + 1);

	if(strstr(buffer, "/") == NULL || strstr(buffer, "/../") != NULL )
	{
		free(buffer);
		return -1;
	}
	memset(rh->fileName, 0, strlen(rh->fileName));
	strncpy(rh->fileName, buffer, strlen(buffer)-strlen(strchr(buffer, ' ')));
	printf("requested filename: %s\n", rh->fileName);
	memset(buffer, 0, sizeof(char)*MAX_RQ_CHUNK_SIZE);

	// получаем тип протокола
	ptrReq = request + strlen(rh->request) + 1 + strlen(rh->fileName) + 1;
	strncpy(buffer, ptrReq, strlen(ptrReq) - strlen(strstr(ptrReq, "\r\n")));

	if(strstr(buffer, "HTTP") == NULL)
	{
		free(buffer);
		return -1;
	}
	memset(rh->protocol, 0, strlen(rh->protocol));
	strncpy(rh->protocol, buffer, strlen(buffer));
	printf("protocol: %s\n", rh->protocol);
	memset(buffer, 0, sizeof(char)*MAX_RQ_CHUNK_SIZE);

    free(buffer);
    return 0;
}

// преобразует response_header в обычную строчку и возвращает указатель на неё
char* get_response(response_header *resp_header) {

	char* res;
	int len_to_string = 0;
	len_to_string = strlen(resp_header->protocol)
	+ strlen(" ")
	+ strlen(resp_header->status)
	+ strlen("\nServer: ")
	+ strlen("\nX-Powered-By: ")
	+ strlen("\nContent-Language: ")
	+ strlen("\nContent-Type: ")
	+ strlen(resp_header->contentType)
	+ strlen("\nContent-Length: ")
	+ strlen(resp_header->contentLength)
	+ strlen("\nConnection: ")
	+ strlen(resp_header->connection)
	+ strlen("\n\n")
	+ strlen(resp_header->content);

	res = new char[len_to_string + 1];
	res[0] = 0;
	strcat(res, resp_header->protocol);
	strcat(res, " ");
	strcat(res, resp_header->status);
	strcat(res, "\nServer: ");
	strcat(res, "\nX-Powered-By: ");
	strcat(res, "\nContent-Language: ");
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

// проверяет является ли файл директорией
bool is_directory(char *name) {
	//функция stat получает информацию о файле или директории,
	//определенном по pathname, и запоминает ее в структуре, на которую
	//указывает buffer.
	struct stat st;
	//st_mode битовая маска для информации о режиме
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
/*
 * возвращает указатель на массив символов -
 * вывод исполняемой программы с идентификатором
 * pip (не более чем MAX_SIZE_RESPONSE)
 */
char* get_string(int pip) {

	char buf[1000];
	char filename[100];
	char timebuf[100];

	int bytesRead = 0;

	memset(buf, 0, sizeof(char) * 1000);
	memset(filename, 0, sizeof(char) * 100);

	/* формируем уникальное время файла */
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[80];
	time (&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer,80,"%d-%m-%Y-%I-%M-%S",timeinfo);

	/* создаем файл с уникальной меткой времени*/
	strcat(filename, "/home/techcode/webserver_updated/service/progExec_timestamp_");
	strcat(filename, buffer);
	strcat(filename, ".txt");

	/* записываем весь вывод исполняемой программы в созданный файл */
	ofstream outfile(filename);

	do {
		bytesRead = read(pip, buf, 1000);
		outfile.write(buf, bytesRead);
		/*if(bytesRead < 1000)
			break;*/
	} while(bytesRead > 0);

	outfile.close();

	/* считываем весь вывод в буфер и передаем указатель */
	ifstream inputstream(filename);
	inputstream.seekg(0, ios::end);
	size_t size = inputstream.tellg();

	char* filebuffer = (char*)calloc(size, sizeof(char));
	inputstream.seekg(0);
	inputstream.read(filebuffer, size);
	inputstream.close();

	//remove(filename);

	return filebuffer;
}

// исполняет указанный скрипт
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
		dup2(fd[1], 1);
		dup2(fd[1], 2);
		close(fd[1]);
		// state() Проверить наличие этого файла, проверить что он исполняемый
		execve(path, args, NULL);
	}
	//ждем завершения скрипта

	close(fd[1]);
	context[0] = get_string(fd[0]);

	/*while ( (pid=wait(&status)) > 0 ) {
		close(fd[1]);
		context[0] = get_string(fd[0]);
	}*/

	return OK;
}

// исполняет бинарный файл
int execute_bin (char *fileName, char **context) {

int fd[2], status;
char *args[]= {NULL};

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
		close(fd[0]); /* в дочернем закрываем */
		dup2(fd[1], 1); /* стандартный вывод */
		dup2(fd[1], 2); /* вывод ошибок и служебных сообщений */
		close(fd[1]);
		execve(fileName, args, NULL);
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

// получает список файлов в директории
int get_dir_list(char *fileName, char **context) {

DIR *dp;
int fd[2];
struct dirent *dirp;

	if((dp = opendir(fileName)) == NULL) {
		return INTERNAL_SERVER_ERROR;
	}
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
	write(fd[1],"</table>\n", strlen("</table>\n"));

	*context = get_string(fd[0]);
	if(closedir(dp) != 0){
		printf("Error! Can't close dir.\n");
		return INTERNAL_SERVER_ERROR;
	}
	if(*context == 0)
		return INTERNAL_SERVER_ERROR;
	return OK;
}

// заполняет структурку ответа
void get_response_header(request_header *rh, response_header *resp_header) {

char *context;
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

    // если затребован файл/исполнение скрипта
    if(strcmp(rh->request, "GET") == 0) {

    	/*
    	 * склеиваем полное имя файла относительно
    	 * указанной рабочей папки сервера
    	 */

    	lengthFullFileName = strlen(rh->fileName) + dir_main.len;
    	fullFileName = (char *)malloc(lengthFullFileName * sizeof(char));

    	strncpy(fullFileName, dir_main.dir, dir_main.len);
    	strncat(fullFileName, rh->fileName, rh->lengthFileName);

    	if(the_end_file_name(rh->fileName, ".pl")) {
    		printf("[perl script]\n");
    		Status = execute_script(fullFileName, &context, perl, uperl);
    	} else if(the_end_file_name(rh->fileName, ".py")) {
    		printf("[python script]\n");
    		Status = execute_script(fullFileName, &context, python, upython);
    	} else if(the_end_file_name(rh->fileName, ".php")) {
    		printf("[php script]\n");
    		Status = execute_script(fullFileName, &context, php, uphp);
    	} else if(the_end_file_name(rh->fileName, ".exe")) {
    		printf("[binary file]\n");
    		Status = execute_bin(fullFileName, &context);
    	} else if(is_directory(fullFileName)) {
    		printf("[dir_listing]\n");
    		Status = get_dir_list(fullFileName, &context);
    	}
    	else {
    		printf("[file_listing]\n");
    		Status = read_file(fullFileName, &context);
    	}

    	char str[16];
        if(Status != OK) {
        	char* errorMsg = "error due execution\n";
        	sprintf(resp_header->contentLength, "%d", strlen(errorMsg));
        	resp_header->content = new char[strlen(errorMsg) + 1];
        	strcpy(resp_header->content, errorMsg);
        	sprintf(str, "%d", Status);
        	strcpy(resp_header->status, str);
        	printf("%s", errorMsg);
        } else {
			printf("execution successful\n");
			strcpy(resp_header->status, "200 OK");
			sprintf(str, "%d", strlen(context));
			strcpy(resp_header->contentLength, str);
			resp_header->content = new char[strlen(context) + 1];
			strcpy(resp_header->content, context);
			free(context);
        }

        free(fullFileName);
        return;
    }
    //в случае ошибки
    strcpy(resp_header->status, "500");
	strcpy(resp_header->contentLength, "0");
	strcpy(resp_header->content, "\n");

    return;
}

char* get_response_from_server(char* buf_request, request_header *rh, response_header *resp_header) {

	//вычленяем тип запроса + протокол + имя запрошенного файла
	int reqHeaderStatus = get_request_header(buf_request, rh);

	if(reqHeaderStatus < 0)
	{
		char* errorMessage = "wrong request header\n";
		sprintf(resp_header->contentLength, "%d", strlen(errorMessage) + 1);
    	resp_header->content = new char[strlen(errorMessage) + 1];
    	strcpy(resp_header->content, errorMessage);
        strcpy(resp_header->status, "500");
        printf("%s", errorMessage);
	} else {
		//заполняем структуру ответа
		get_response_header(rh, resp_header);
	}

	//вернём всё в виде строки
	char* buf_response = get_response(resp_header);
	return buf_response;
}

/*
 * отправляем содержимое buf_response
 * в буфер сокета
 */
void answer_to_display(char* buf_response, int sock) {

char b[2];

	for(int i = 10; i < 13; i++)
		b[i-10] = buf_response[i];
	int value_status = atoi(b);

	char buf_answer_display[MAX_SIZE];
	if(value_status == NOT_FOUND) {
	   strcpy(buf_answer_display, "404\nNot Found.");
	   send(sock, buf_answer_display, strlen(buf_answer_display), 0);
	}
	else if(value_status == INTERNAL_SERVER_ERROR) {
	   strcpy(buf_answer_display, "500\nInternal Server Error.");
	   send(sock, buf_answer_display, strlen(buf_answer_display), 0);
	}

	else
	{
		int t = strlen(buf_response);
		// посылаем данные в сокет
		send(sock, buf_response, strlen(buf_response), 0);
	}



	return;
}

// выполняем функцию fn в отдельном потоке со сформированным стеком newstack
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

/*
 * основная функция-обработчик потоков
 * передается индекс (один и тот же для массивов мьютексов и
 * для массивов сокетов
 */

void * child_main(void *i) {

int j = (int)i;
request_header rh;
response_header resp_header;

	// выделяем память под данные, на которые будет ссылаться структура запроса
	rh.request   = new char[20];
	rh.fileName  = new char[200];
	rh.protocol  = new char[20];

	// выделяем память под данные, на которые будет ссылаться структура ответа
	resp_header.protocol        = new char[MAX_SIZE];
	resp_header.status          = new char[MAX_SIZE];
	resp_header.contentType     = new char[MAX_SIZE];
	resp_header.contentLength   = new char[MAX_SIZE];
	resp_header.connection      = new char[MAX_SIZE];

	printf("current pid: %d | index: %d | status: idling\n", getpid(), j);
	while(1) {

		// дожидаемся выставление мютекса в signals_array[j], сразу же лочимся и и начинаем
		// обрабатывать client_socks[j]
		pthread_mutex_lock(&signals_array[j]);
		printf("thread index: %d, socket %d in progress\n", j, client_socks[j]);
		char buf_request[MAX_SIZE_RESPONSE];

		// получаем запрос
		int size_of_result = recv(client_socks[j], buf_request, MAX_SIZE_RESPONSE, 0);//вернет значение равное, тому сколько передаст
		buf_request[size_of_result] = 0;
		printf("size_of_result = %d Request:\n %s\n", size_of_result, buf_request);

		// формируем ответ
		char* buf_response = get_response_from_server(buf_request, &rh, &resp_header);
		answer_to_display(buf_response, client_socks[j]);

		if(close(client_socks[j]) != 0)
			printf("error: can't close socket %d.", client_socks[j]);

		delete [] buf_response;
		delete [] resp_header.content;
		pthread_mutex_unlock(&states_array[j]);
	}

	return NULL;
}

int main(int argc, char *argv[], char *envp[]) {

int p = 0, result = 0, rc, sock2, status;
pthread_t thread;
struct sockaddr_in addr;
struct sockaddr_in addr2;

	/*
	 * обрабатываем переданные при запуске программы агрументы
	 * -p<порт> -d<рабочая папка программы>
	 */
	while ((result = getopt(argc, argv, "p:d:")) != -1) {
		switch (result) {
			case 'p':
				p = atoi(optarg);
				break;
			case 'd':
				/*
				 * dir_main - объект структуры, описывающий каталог
				 * заполняем длину и имя из переданных параметров
				 */
				dir_main.len = strlen(optarg) + 1;
				dir_main.dir = (char*)malloc(dir_main.len);
				strcpy(dir_main.dir, optarg);
				break;

	    };
	}
	printf("server started on port: %d\n", p);
	printf("working directory: %s\n\n", dir_main.dir);

	/*
	 * создаем NUM_OF_PROCESSES процессов для обработки запросов сервера
	 * child_main - функция обработчик для каждого из запросов
	 */
	for(int i = 0; i < NUM_OF_PROCESSES; ++i) {
		pthread_mutex_lock(&signals_array[i]);
		pthread_mutex_unlock(&states_array[i]);
		status = pthread_create(&thread, NULL, child_main, (void *)i);
		if (status != 0) {
			printf("main error: can't create thread, status = %d\n", status);
			return 1;
		}
	}

    socklen_t size = sizeof (addr2);
    // создаем сокет
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    printf("new socket created: %i\n", sock);
    memset(&addr, 0, sizeof (addr));

    // заполняем структурку addr типа sockaddr_in
    addr.sin_family = AF_INET; // иден. домена
    addr.sin_port = htons(p);  // порт
    addr.sin_addr.s_addr = 0;  // любой

    // связываем сокет с адресом
    if(bind(sock, (struct sockaddr*)&addr, sizeof (struct sockaddr_in)) == -1) {
    	printf("socket binding error: %d\n", errno);
    	return 1;
    }
    else
    	printf("socked binded successfully!\n");
    // слушаем сокет
    listen(sock, MAX_SIZE);

    // ждем соедениея с сервером
    while(1) {
    	// соединяемся с клиентом, обрабатываем его команды
        sock2 = accept(sock, (struct sockaddr*)&addr2, &size);
        // возникла ошибка
        if(sock2 == -1) {
            printf("error %d: can't create client socket.\n", errno);
            return 1;
        }
        printf("client socket created, socket: %i\n", sock2);

        /*
         * пробегаем по всем запущенным процессам, ищем свободный/
         * ждем, пока какой-либо из процессов не освободится
         */
        int i = 0;
        while(1) {
        	if(i == NUM_OF_PROCESSES)
        		i = 0;
        	/* залоченное состояние signals_array[i] сигнализирует что процесс свободен */
        	if ( (rc = pthread_mutex_trylock(&signals_array[i])) == EBUSY ) {
        		if ( (rc = pthread_mutex_trylock(&states_array[i])) == 0 ) {
        			/*
        			 * нашли свободный процесс - выставили в signals_array что
        			 * он готов к выполнению и поместили сокет в массив обрабатываемых сокетов
        			 * client_socks
        			 */
        			printf("server thread index: %d\n", i);
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
