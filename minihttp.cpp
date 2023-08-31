#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

//cpp
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#define SERVER_PORT 1111

static int debug = 1;

int get_line(int sock, char *buf,int size);
void* do_http_request(void * pclient_sock);
void do_http_response(int client_sock, const char *path);
int headers(int client_sock, FILE *resource, const std::string &fileType);
void cat(int client_sock, FILE *resource);

void not_found(int client_sock);//404
void unimplemented(int client_sock);//500
void bad_request(int client_sock);//400
void inner_error(int client_sock);
std::string getType(std::string s);

int main(void){
	int sock;//email box
	struct sockaddr_in server_addr;

	//create email box
	sock = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&server_addr, sizeof(server_addr));

	server_addr.sin_family = AF_INET;//select IPV4
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(SERVER_PORT);

	//complete sign->email box
	bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

	//set the number of accept in the same time
	listen(sock, 128);

	printf("waiting the connetion of port...\n");

	int done = 1;
	while(done) {
		struct sockaddr_in client;
		int client_sock, len;
		char client_ip[64];
		char buf[256];
		pthread_t id;
		int *pclient_sock = NULL;
		socklen_t client_addr_len;
		client_addr_len = sizeof(client);
		client_sock = accept(sock, (struct sockaddr *)&client, &client_addr_len);

		printf("client ip: %s\t port : %d\n",
			inet_ntop(AF_INET, &client.sin_addr.s_addr,client_ip,sizeof(client_ip)),
			ntohs(client.sin_port));

		//do http request
		//run thread to do http request
		pclient_sock = (int*)malloc(sizeof(int));
		*pclient_sock = client_sock;
		pthread_create(&id, NULL, do_http_request, (void*)pclient_sock);
		// do_http_request(client_sock);
		
	}
	close(sock);
	return 0;
}
void * do_http_request(void * pclient_sock){
	int len = 0;
	char buf[256];
	char method[64];
	char url[256];
	char path[512];
	int client_sock = *(int *)pclient_sock;
	struct stat st;
	//read http request from client

	//1.read request line
	int i = 0, j = 0;
	//get request line
	len = get_line(client_sock, buf, sizeof(buf));
	if(len > 0) {
		while(!isspace(buf[j]) && i < sizeof(method)-1) {
			method[i++]=buf[j++];
		}
		method[i] = '\0';
		printf("buf:%s\n",buf);
		printf("request method: %s\n",method);
		if(strcasecmp(method, "GET") == 0) {
			if(debug) printf("method = GET\n");
			//get url
			while(isspace(buf[j++]));
			i = 0;
			while(!isspace(buf[j]) && i < sizeof(method)-1) {
				url[i++]=buf[j++];
			}
			url[i] = '\0';

			if(debug)printf("url: %s\n", url);
			//get http head
			do{
				len = get_line(client_sock, buf, sizeof(buf));
				if(debug) printf("read: %s\n", buf);
			}while(len>0);
			//locate local html file
			//handle '?' in the url
			{
				char *pos = strchr(url, '?');
				if(pos) {
					*pos='\0';
					printf("real url:%s\n", url);
				}
			}
			sprintf(path, "./html_docs/%s", url);
			if(debug) printf("path: %s\n", path);
			//do http response
			//if file exist ,respons;else return error;
			if(stat(path,&st) == -1) {
				//file not exist
				fprintf(stderr,"stat %s failed. reason: %s\n",path,strerror(errno));
				not_found(client_sock);
			}else{
				if(S_ISDIR(st.st_mode)){
					strcat(path, "/index.html");
				}
				do_http_response(client_sock, path);
			}
			
		}else {//read http head and return 501: method not implemented
			fprintf(stderr,"waring! other request [%s]\n",method);
			do{
				len = get_line(client_sock, buf, sizeof(buf));
				if(debug) printf("read: %s\n", buf);
			}while(len>0);
			unimplemented(client_sock);
		}

	}else {//format error
		bad_request(client_sock);
	}
	printf("------------------");
	time_t timer=time(NULL);
	char *pctime = ctime(&timer);
	printf(" %s ",pctime);
	close(client_sock);
	if(pclient_sock) free(pclient_sock);
	return NULL;
}

void do_http_response(int client_sock, const char *path) {
	FILE *resource = NULL;
	std::string fileType = path;
	fileType = getType(fileType);
	std::cout<<"fileType:"<<fileType<<'\n';
	if(fileType == "jpg"){
		std::cout<<"rb:"<<'\n';
		resource = fopen(path, "rb");
	}else{
		std::cout<<"r:"<<'\n';
		resource = fopen(path, "r");
	}
	
	if(resource == NULL) {
		not_found(client_sock);
		return ;
	}
	//1.send http head
	if(headers(client_sock, resource, fileType) == 0){
		//2.send http body
		cat(client_sock, resource);
	}

	fclose(resource);
}
int headers(int client_sock, FILE *resource, const std::string &fileType){

	struct stat st;
	int fileid = 0;
	char tmp[64];
	char buf[1024] = {0};
	strcat(buf, "HTTP/1.0 200 OK\r\n");
	strcat(buf, "Server: Martin Server\r\n");
	if(fileType == "jpg"){
		strcat(buf, "Content-Type: jpg\r\n");
	}else{
		strcat(buf, "Content-Type: text/html\r\n");
	}
	strcat(buf, "Connection: Close\r\n");

	fileid = fileno(resource);

	if(fstat(fileid, &st) == -1) {
		inner_error(client_sock);
		// not_found(client_sock);
		return -1;
	}
	snprintf(tmp,64,"Content-Length: %ld\r\n\r\n", st.st_size);
	strcat(buf, tmp);
	// st.st_size;
	if(debug) fprintf(stdout, "header: %s\n", buf);
	if(send(client_sock, buf, strlen(buf), 0) == -1) {
		fprintf(stderr, "send failed. data:%s, reason: %s\n", buf, strerror(errno));
		return -1;
	}
	return 0;
}
void cat(int client_sock, FILE *resource){
	//verson 1.0 X
	// char buf[1024];
	// int len;
	// fgets(buf, sizeof(buf), resource);
	// while(!feof(resource)){
	// 	len = send(client_sock, buf, strlen(buf),0);
	// 	if(len == -1) {
	// 		fprintf(stderr, "send body error. reason: %s\n", strerror(errno));
	// 		break;
	// 	}
	// 	if(debug) fprintf(stdout, "%s", buf);
	// 	fgets(buf, sizeof(buf), resource);
	// }

	//version 2.0 X
	// char buf[1024];
	// char *status;
	// while(1){
	// 	status = fgets(buf, sizeof(buf), resource);
		
	// 	if(status == NULL) {
	// 		break;
	// 	}
	// 	send(client_sock, buf, strlen(buf),0);
	// }

	//version 3.0
	//https://www.bilibili.com/video/BV1B24y1d7Vi?p=13&vd_source=8924ad59b4f62224f165e16aa3d04f00
	char buff[4096];
	int cnt = 0;
	while(1) {
		int ret = fread(buff, sizeof(char), sizeof(buff), resource);
		if(ret <= 0){
			break;
		}
		send(client_sock, buff, ret, 0);
		cnt += ret;
	}
	printf("had send %d byte to browser\n",cnt);

}
//return value:
//-1: error occurs
//0 : read an empty line
//>0: read success
int get_line(int sock, char *buf,int size){
	int count = 0;
	char ch = '\0';
	int len = 0;

	while( (count < size-1) && (ch != '\n')) {
		len = read(sock, &ch, 1);

		if(len == 1) {
			if(ch == '\r'){
				continue;
			}else if(ch == '\n'){
				//buf[count] = '\0'
				break;
			}
			buf[count] = ch;
			count++;

		}else if(len == -1) {
			perror("read failed");
			count = -1;
			break;
		}else {//len==0,client closed sock
			fprintf(stderr,"client close.\n");
			count = -1;
			break;
		}
	}
	if(count >= 0){
		buf[count] = '\0';
	}
	return count;
}

std::string getType(std::string s){
	char c='.';
	std::vector<std::string> res;
	std::stringstream ss;
	ss<<s;
	while(getline(ss,s,c)){
		res.push_back(s);
	}
	if(res.size() < 2){
		return "error";
	}
	return res.back();
}
void not_found(int client_sock){

	const char *reply = "HTTP/1.0 404 NOT FOUND\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\r\n\
<HEAD>\r\n\
<TITLE>NOT FOUND</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>FILE NOT EXIST! \r\n\
	<p>THE server could not fulfill your request because the resource specified is unavailable or nonexistent.\r\n\
</BODY>\r\n\
</HTML>";
	int len = write(client_sock, reply, strlen(reply));
	if(debug == 1) fprintf(stdout,"%s",reply);

	if(len <= 0) {
		fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
	}
}

void unimplemented(int client_sock){

	const char *reply = "HTTP/1.0 500 Method Not IMplemented\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>Method Not Implemented</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>HTTP request method not supported. \r\n\
</BODY>\r\n\
</HTML>";
	int len = write(client_sock, reply, strlen(reply));
	if(debug == 1) fprintf(stdout,"%s",reply);

	if(len <= 0) {
		fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
	}
}

void bad_request(int client_sock){

	const char *reply = "HTTP/1.0 500 Method Not IMplemented\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>BAD REQUEST</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>Your browser sent a bad request! \r\n\
</BODY>\r\n\
</HTML>";
	int len = write(client_sock, reply, strlen(reply));
	if(debug == 1) fprintf(stdout,"%s",reply);

	if(len <= 0) {
		fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
	}
}

void inner_error(int client_sock){
	const char *reply="HTTP/1.0 500 Internal Server Error\r\n\
Content-Type: text/html\r\n\
\r\n\
<html>\
<head>\
<title> Method NOt IMplemented</title>\
</head>\
<body>\
	<p>Error prohibited CGI execution.\
</body>\
</html>";
	int len = write(client_sock, reply, strlen(reply));
	if(debug == 1) fprintf(stdout, "%s", reply);
	if(len <= 0){
		fprintf(stderr," send reply failed. reason: %s\n", strerror(errno));
	}
}
