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
#include <map>
#define SERVER_PORT 1111

//mysql
#include <mysql/mysql.h>
class Mysql{
private:
	MYSQL *con;
	std::string host = "127.0.0.1";
	std::string user = "root";
	std::string pw = "lemonTREE41799!";
	std::string database_name = "test";
	int port = 3306;
	Mysql(){
		con = mysql_init(NULL);
		// mysql_options(con,MYSQL_SET_CHARSET_NAME,"GBK");
		if(!mysql_real_connect(con, host.c_str(), user.c_str(), pw.c_str(), database_name.c_str(), port, NULL, 0)){
			fprintf(stderr, "Failed to connetct to database: ERROR%s\n", mysql_error(con));
			exit(1);
		}
	}
	~Mysql(){
		mysql_close(con);
	}
	std::string f(std::string s) {
		return "\"" + s + "\"";
	}
public:

	static Mysql* getInstance(){
		static Mysql mysql;
		return &mysql;
	}
	//-1:not empty 1:error 0:succeeded
	int insert_(std::string s1,std::string s2){
		std::string sql;
		//insert into user values("11","22");
		sql = "insert into user values(\"" + s1 + "\",\"" + s2 + "\");";
		std::cout<<"insert sql:"<<sql<<'\n';
		if(!query_(s1).empty()){
			return -1;
		}
		// std::cout<<"hello\n";
		//mysql_query(): if succeeded, return 0.

		if(mysql_query(con, sql.c_str())){
			fprintf(stderr, "Failed to insert data ERROR: %s\n", mysql_error(con));
			return 1;
		}
		// std::cout<<"hello\n";
		return 0;
	}
	int delete_(std::string s1){
		std::string sql;
		//insert into user values("11","22");
		sql = "delete from user where name = \"" + s1 + "\";";
		
		//mysql_query(): if succeeded, return 0.
		if(mysql_query(con, sql.c_str())){
			fprintf(stderr, "Failed to delete data ERROR: %s\n", mysql_error(con));
			return 1;
		}
		return 0;
	}

	int update_(std::string s1,std::string s2){
		std::string sql;
		//update user pw = "222" where name = "111";
		sql = "update user set pw = \"" + s2 + "\" where name = \"" + s1 + "\";";
		// std::cout<<"sql:"<<sql<<'\n';
		//mysql_query(): if succeeded, return 0.
		if(mysql_query(con, sql.c_str())){
			fprintf(stderr, "Failed to update data ERROR: %s\n", mysql_error(con));
			return 1;
		}
		return 0;
	}

	std::vector<std::pair<std::string,std::string>> query_(std::string s1="") {
		std::string sql;
		if(s1 == ""){
			sql = "select *from user";
		}else {
			sql = "select *from user where name = " + f(s1) +";";
		}
		std::cout<<"query sql:"<<sql<<'\n';
		if(mysql_query(con, sql.c_str())){
			fprintf(stderr, "Failed to query data ERROR: %s\n", mysql_error(con));
			return {};
		}
		MYSQL_RES *res = mysql_store_result(con);

		std::vector<std::pair<std::string,std::string>> ret;
		MYSQL_ROW row;
		while(row = mysql_fetch_row(res)) {
			std::pair<std::string,std::string> t;
			t.first = row[0];
			t.second = row[1];
			ret.push_back(t);
		}
		return ret;
	}
};


static int debug = 1;

int get_line(int sock, char *buf,int size);
void* do_http_request(void * pclient_sock);
void do_get_response(int client_sock, const char *path);
void do_post_response(int client_sock, std::string log_reg, std::string &s1, std::string &s2);
int headers(int client_sock, FILE *resource, const std::string &fileType);
void cat(int client_sock, FILE *resource);

//return html
void user_or_psw_cant_be_empty(int client_sock);//400
void not_found(int client_sock);//404
void unimplemented(int client_sock);//500
void bad_request(int client_sock);//400
void inner_error(int client_sock);//500
//other
std::string getType(std::string s);
//insert
// int insert(std::string &s1,std::string &s2);


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
	{
	/*
	http request format:
	1.request line 2.reqeust head 3.request date

	like this:
	http request
	{
		request line
		requet head
		{
			...
			...
		}
		'\r\n'
		request date
	}

	there is a '\r\n' between 'request head' and 'request date'
	so I need to use 'get_line()' twice more after read 'request head'.
	(cause I am certain it is only two line 'request data' in here)
	*/
	}

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
		//get method
		while(!isspace(buf[j]) && i < sizeof(method)-1) {
			method[i++] = buf[j++];
		}
		method[i] = '\0';
		printf("buf:%s\n", buf);
		printf("request method: %s\n", method);

		//get url
		while(isspace(buf[j++]));
		i = 0;
		while(!isspace(buf[j]) && i < sizeof(method)-1) {
			url[i++]=buf[j++];
		}
		url[i] = '\0';
		if(debug)printf("request url: %s\n", url);
		//2.read http head
		do{
			len = get_line(client_sock, buf, sizeof(buf));
			if(debug) printf("read: %s\n", buf);
		}while(len>0);
		if(strcasecmp(method, "GET") == 0) {
			char *pos = strchr(url, '?');
			if(pos) {
				*pos='\0';
				printf("real url:%s\n", url);
			}
			
			sprintf(path, "./html_docs/%s", url);
			if(debug) printf("path: %s\n", path);
			//do http response
			//if file exist ,respons;else return error;
			if(stat(path,&st) == -1) {
				//file not exist
				fprintf(stderr, "stat %s failed. reason: %s\n", path, strerror(errno));
				not_found(client_sock);
			}else{
				if(S_ISDIR(st.st_mode)){
					strcat(path, "/index.html");
				}
				do_get_response(client_sock, path);
			}
			//method 'GET' has no 'request data' frequently.
		}else if(strcasecmp(method, "POST") == 0) {
			//3.read request data.
			len = get_line(client_sock, buf, sizeof(buf));
			if(debug) printf("read: %s len:%d\n", buf,(int)strlen(buf));
			std::string s1 = buf;

			len = get_line(client_sock, buf, sizeof(buf));
			if(debug) printf("read: %s len:%d\n", buf,(int)strlen(buf));
			std::string s2 = buf;

			len = get_line(client_sock, buf, sizeof(buf));
			if(debug) printf("read: %s len:%d\n", buf,(int)strlen(buf));
			std::string log_reg = buf;

			s1 = s1.substr(s1.find('=')+1);
			s2 = s2.substr(s2.find('=')+1);
			log_reg = log_reg.substr(log_reg.find('=')+1);
			if(s1.empty() or s2.empty()) {
				user_or_psw_cant_be_empty(client_sock);
				return NULL;
			}else {
				do_post_response(client_sock, log_reg, s1, s2);
			}
		}else {//read http head and return 501: method not implemented
			fprintf(stderr,"warning! other request! [%s]\n",method);
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

void do_get_response(int client_sock, const char *path) {
	FILE *resource = NULL;
	std::string fileType = path;
	fileType = getType(fileType);
	std::cout<<"fileType:"<<fileType<<'\n';
	if(fileType == "html") {
		std::cout<<"r:"<<'\n';
		resource = fopen(path, "r");
	}else{
		std::cout<<"rb:"<<'\n';
		resource = fopen(path, "rb");
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
//user_not_exist();
//turn_to_first_page();
//wrong_password();
//register_succeeded();
void do_post_response(int client_sock, std::string log_reg, std::string &s1, std::string &s2){
	std::cout<<log_reg<<'\n';
	auto t = Mysql::getInstance()->query_(s1);
	std::cout<<"empty:"<<t.empty()<<'\n';
	if(log_reg == "login") {
		std::cout<<"login\n";
		if(t.empty()) {
			//user_not_exist();
			do_get_response(client_sock, "./html_docs/user_not_exist.html");
		}else {
			if(t[0].second == s2) {
				std::cout<<"go to firstpage.html";
				do_get_response(client_sock, "./html_docs/firstpage.html");
			}else {
				do_get_response(client_sock, "./html_docs/wrong_password.html");
				// wrong_password();
			}
		}
	}else if(log_reg == "register") {
		std::cout<<"register\n";
		if(t.empty()) {
			Mysql::getInstance()->insert_(s1, s2);
			//register succeeded
		}else {
			//user name has been used;
		}
	}else {
		std::cout<<"a ha ?\n";
	}
}

int headers(int client_sock, FILE *resource, const std::string &fileType){
	struct stat st;
	int fileid = 0;
	char tmp[64];
	char buf[1024] = {0};
	strcat(buf, "HTTP/1.0 200 OK\r\n");
	strcat(buf, "Server: Martin Server\r\n");
	if(fileType == "jpg") {
		strcat(buf, "Content-Type: image/jpeg\r\n");
	}else if(fileType == "mp4") {
		strcat(buf, "Content-Type: video/mp4\r\n");
	}
	else {
		strcat(buf, "Content-Type: text/html\r\n");
	}
	strcat(buf, "Connection: Keep-Alive\r\n");

	fileid = fileno(resource);

	if(fstat(fileid, &st) == -1) {
		inner_error(client_sock);
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
	{
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
	}
	//version 3.0
	//https://www.bilibili.com/video/BV1B24y1d7Vi?p=13&vd_source=8924ad59b4f62224f165e16aa3d04f00
	char buff[4096];
	int cnt = 0;
	int ret;
	while(1) {
		ret = fread(buff, sizeof(char), sizeof(buff), resource);
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
void user_or_psw_cant_be_empty(int client_sock){

	const char *reply = "HTTP/1.0 400 CAN BE EMPTY\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\r\n\
<HEAD>\r\n\
<TITLE>CANT BE EMPTY</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>USERNAME OR PASSWORD CAN BE EMPTY \r\n\
	<p>please input username and password.\r\n\
</BODY>\r\n\
</HTML>";
	int len = write(client_sock, reply, strlen(reply));
	if(debug == 1) fprintf(stdout,"%s",reply);

	if(len <= 0) {
		fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
	}
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

// int insert(std::string &s1,std::string &s2){
// 	// MYSQL *con;
// 	// mysql_init(NULL);
// 	MYSQL *con = mysql_init(NULL);
// 	// printf("hello\n");
// 	// mysql_options(con, MYSQL_SET_CHARSET_NAME, "GBK");

// 	if(!mysql_real_connect(con, host, user, pw, database_name, port, NULL, 0)){
// 		fprintf(stderr, "Failed to connetct to database: ERROR%s\n", mysql_error(con));
// 		return -1;
// 	}
// 	std::string sql;
// 	//insert into user values("11","22");
// 	sql = "insert into user values(\"" + s1 + "\",\"" + s2 + "\");";
// 	if(mysql_query(con, sql.c_str())){
// 		fprintf(stderr, "Failed to insert data: ERROR%s\n", mysql_error(con));
// 		return -1;
// 	}

// 	mysql_close(con);
// 	return 0;
// }
