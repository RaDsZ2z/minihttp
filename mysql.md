
# 1.两个账号：

username:root
password:lemonTREE41799!

username:test
password:teseTEST111!

下面这个账号没有什么权限(还没有研究怎么修改权限)

# 2.C/CPP 使用 mysql.h
```
安装libmysqlclient-dev
https://blog.csdn.net/qq_51048314/article/details/125380099
编译指令
gcc test.c -o run -L/user/lib/mysql -lmysqlclient
```

# 3.mysql.h

```
https://blog.csdn.net/A_salted_fisher/article/details/131117697
```

# 4.增删改(类似?)
```cpp
// #include <iostream>
#include <mysql/mysql.h>

#include <string.h>
#include <stdio.h>
// using namespace std;
const char *host = "127.0.0.1";
const char *user = "root";
const char *pw = "lemonTREE41799!";
const char *database_name = "test";
const int port = 3306;
int main(){
	// MYSQL *con;
	// mysql_init(NULL);
	MYSQL *con = mysql_init(NULL);
	printf("hello\n");
	mysql_options(con, MYSQL_SET_CHARSET_NAME, "GBK");

	if(!mysql_real_connect(con, host, user, pw, database_name, port, NULL, 0)){
		fprintf(stderr, "Failed to connetct to database: ERROR%s\n", mysql_error(con));
		return -1;
	}
	char sql[256];
	sprintf(sql,"insert into test values(2,\"333\");");
	if(mysql_query(con, sql)){
		fprintf(stderr, "Failed to insert data: ERROR%s\n", mysql_error(con));
		return -1;
	}

	mysql_close(con);
	return 0;
}
```
