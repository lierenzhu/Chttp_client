#include "http.h"
#include "http_parser.h"
#include "cJSON.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h> 
#include <ctype.h>
#include <netdb.h>
#include <strings.h>

//#ifdef PLATFORM_LINUX
	#define _strdup strdup
	#define _stricmp strcasecmp
	#define _strnicmp strncasecmp
//#else 
//	#define _strdup strdup
//	#define _stricmp strcmp
//	#define _strnicmp strncmp
//#endif

const int kSelectRead	= 1 << 0;
const int kSelectWrite	= 1 << 1;
const int kSelectError	= 1 << 2;

#define CONNECT_STR "Connection: close\r\n"
#define ACCEPT_STR "Accept: */*\r\n"
#define CONTENT_LENGTH_STR "Content-Length"
#define CONTENT_TYPE_STR "Content-Type:application/json\r\n"
#define CONTENT_DISPOSITION_STR "Content-Disposition"
#define CRLF "\r\n"

enum parser_statue_e { PARSERD_NONE = 0, PARSERD_FIELD, PARSERD_VALUE, PARSERD_BODY };

enum proto_type_e { PROTO_HTTP = 0};


typedef int socket_t;
#define HTTP_INVALID_SOCKET -1
#define HTTP_EINTR EINTR
#define HTTP_EINPROGRESS EINPROGRESS
#define HTTP_EWOULDBLOCK EWOULDBLOCK
#define HTTP_EALREADY EALREADY


#define RECV_BUF_SIZE 4 * 1024

struct ft_http_client_t
{
	FILE* pf;
	char* filename;
	char* body;
	char* redirect_url;
	char* header_field;
	char* header_value;

	char* user;
	data_recv_cb_t recv_cb;
	
	unsigned long body_len;
	unsigned long content_length;

	enum http_request_method_e method;
	enum proto_type_e proto_type;

	unsigned short field_size;
	unsigned short value_size;
	unsigned short cur_field_size;
	unsigned short cur_value_size;



	socket_t fd; 
	int timeout;
	
	short status_code;
	char parser_statue;
	char error_code;
	unsigned cancel	  : 1;
	unsigned exit	  : 1;
	unsigned download : 1;
	unsigned redirect : 1;
};


#define socket_close close


#define free_member(member) if((member)) { free(member); (member) = NULL; }
#define close_socket(fd) if(fd != HTTP_INVALID_SOCKET) { socket_close(fd); fd = HTTP_INVALID_SOCKET; }
#define close_file(pf) if(pf != NULL) { fclose(pf); pf = NULL; }



HTTP_API int ft_http_init()
{
	return 0;
}

HTTP_API void ft_http_deinit()
{

}

HTTP_API ft_http_client_t* ft_http_new()
{
	ft_http_client_t* http = (ft_http_client_t*)calloc(1, sizeof(ft_http_client_t));

	return http;
}


HTTP_API void ft_http_destroy(ft_http_client_t* http)
{
	if(http == NULL) return;
	
	free_member(http->body);
	free_member(http->header_field);
	free_member(http->header_value);
	free_member(http->redirect_url);
	free_member(http->filename);
	close_socket(http->fd);
	close_file(http->pf);

	free(http);

}

HTTP_API int ft_http_get_error_code(ft_http_client_t* http)
{
	if(http == NULL)
	{
		return -1;
	}
	return http->error_code;
}

HTTP_API int ft_http_get_status_code(ft_http_client_t* http)
{
	if(http == NULL)
	{
		return -1;
	}
	return http->status_code;
}

HTTP_API int ft_http_set_timeout(ft_http_client_t* http, int timeout)
{
	if(http == NULL)
	{
		return -1;
	}
	if(timeout < 0) 
	{
		http->timeout = 0;
	}
	else
	{
		http->timeout = timeout;
	}

	return 0;
}

static int socket_noblocking(socket_t fd, int noblocking)
{

	int flags;
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
	{
		return -1;
	}
	if(noblocking)
	{
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}
	else
	{
		return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
	}

	return 0;
}

static int last_error()
{
//#ifdef PLATFORM_LINUX
	return errno;
//#endif
	return 0;
}


static int socket_select(ft_http_client_t* http, int mode, int timeout)
{
	fd_set rfd, wfd, efd;
	int r = 0;
	int error = 0;
	int remaind = timeout;

	socklen_t len = sizeof(error);

	struct timeval tv, start, elapsed;

	gettimeofday(&start, 0);

	while(remaind > 0)
	{
		 if(mode & kSelectRead)	{ FD_ZERO(&rfd); FD_SET(http->fd, &rfd); }
		 if(mode & kSelectWrite){ FD_ZERO(&wfd); FD_SET(http->fd, &wfd); }
		 if(mode & kSelectError){ FD_ZERO(&efd); FD_SET(http->fd, &efd); }

		tv.tv_sec = remaind / 1000;
		tv.tv_usec = remaind % 1000 * 1000;

		r = select(http->fd+1,
			(mode & kSelectRead)	? &rfd : NULL,
			(mode & kSelectWrite)	? &wfd : NULL,
			(mode & kSelectError)	? &efd : NULL,
			&tv);

		if( r == 0)
		{
			return -1; /* timeout */
		}

		if( r > 0) 
		{
			if(getsockopt(http->fd, SOL_SOCKET, SO_ERROR, (char*)&error, &len) == 0 && error == 0)
			{
				return 0;
			}
			else
			{
				return -1;
			}
		}

		if( r < 0 )
		{
			if(last_error() == HTTP_EINTR)
			{
				gettimeofday(&elapsed, 0);
				remaind = timeout - ((int)(elapsed.tv_sec - start.tv_sec) * 1000 + (int)(elapsed.tv_usec - start.tv_usec) / 1000);

				continue;
			}
			else
			{
				return -1;
			}
		}
	};

	return -1;
}



static int _field_value_malloc(char** str, unsigned short* cur_size, unsigned short* size, const char* at, size_t length)
{
	if(*str == NULL)
	{
#define DEFAULT_HEADER_SIZE 128
		*size = length > DEFAULT_HEADER_SIZE ? length: DEFAULT_HEADER_SIZE;
		*str = (char*)calloc(1, *size + 1);
		if(*str == NULL)
		{
			return -1;
		}
		*cur_size = 0;
	}
	else if(*cur_size + length > *size)
	{
		*size = *cur_size + length;
		*str = (char*)realloc(*str, *size + 1);
		if(*str == NULL)
		{
			return -1;
		}
	}
	memcpy(*str + *cur_size, at, length);
	*cur_size += length;
	(*str)[*cur_size] = '\0';
	return 0;
}

static int parser_field_value(ft_http_client_t* http)
{
	if(http->cur_value_size > 0 && 
		http->cur_field_size > 0 &&
		http->header_field && http->header_value)
	{
		if(_stricmp(http->header_field, "Location") == 0)
		{
			free_member(http->redirect_url);
			http->redirect_url = _strdup(http->header_value);

			http->redirect = 1; return -1;
		}
		else if(_stricmp(http->header_field, CONTENT_LENGTH_STR) == 0)
		{
			http->content_length = atol(http->header_value);
		}
		else
		{
			/* extract other header field value */
		}
		http->cur_field_size = 0;
		http->cur_value_size = 0;
	}

	return 0;
}

static int on_header_field_cb(http_parser* parser, const char *at, size_t length)
{
	ft_http_client_t* http = (ft_http_client_t*)parser->data;

	if(http->parser_statue == PARSERD_VALUE)
	{
		if( parser_field_value(http) != 0)
		{
			return -1;
		}
	}
	http->parser_statue = PARSERD_FIELD;
	return _field_value_malloc(&http->header_field, &http->cur_field_size, &http->field_size, at, length);
}

static int on_header_value_cb(http_parser* parser, const char *at, size_t length)
{
	ft_http_client_t* http = (ft_http_client_t*)parser->data;

	if(http->parser_statue == PARSERD_FIELD || http->parser_statue == PARSERD_VALUE)
	{
		http->parser_statue = PARSERD_VALUE;
		return _field_value_malloc(&http->header_value, &http->cur_value_size, &http->value_size, at, length);
	}
	return 0;
}

static int on_status_cb(http_parser* parser, const char *at, size_t length)
{
	ft_http_client_t* http = (ft_http_client_t*)parser->data;
	http->status_code = parser->status_code;
	return 0;
}

static int on_headers_complete_cb(http_parser* parser)
{
	ft_http_client_t* http = (ft_http_client_t*)parser->data;
	if(parser_field_value(http) != 0)
	{
		return -1;
	}
	free_member(http->header_field);
	free_member(http->header_value);
	http->parser_statue = PARSERD_BODY;
	http->cur_field_size = http->cur_value_size = 0;
	return 0;
}

static int on_download_file_cb(http_parser* parser, const char *at, size_t length)
{
	ft_http_client_t* http = (ft_http_client_t*)parser->data;

	if(http->status_code >= 200 && http->status_code <= 299)
	{
		if(http->pf == NULL)
		{
			if(http->filename != NULL)
			{
				http->pf = fopen(http->filename, "wb");
			}
		}

		if(http->pf == NULL)
		{
			return -1;
		}


		/* report download progress */
		if( http->recv_cb && (http->recv_cb)(http, at, length, http->content_length, http->user) != 0)
		{
			return -1;
		}

		fwrite(at, 1, length, http->pf);
	}

	return 0;
}


static int on_body_cb(http_parser* parser, const char *at, size_t length)
{
	ft_http_client_t* http = (ft_http_client_t*)parser->data;
	
	if(http->body == NULL)
	{
		
		if(http->content_length > 0)
		{
			/* 直接一次分配足够空间，避免多次分配 */
			http->body = (char*)calloc(1, http->content_length + 1);
		}
		else
		{
			http->body = (char*)calloc(1, length + 1);
		}
	}
	else
	{
		if(http->content_length <= 0)
		{
			http->body = (char*)realloc(http->body, http->body_len + length + 1);
		}
	}
	if(http->body == NULL)
	{
		return -1;
	}
	memcpy(http->body + http->body_len, at, length);
	http->body_len += length;


	return 0;
}

static int on_message_complete_cb(http_parser* parser)
{
	return 0;
}

static int http_check_error(ft_http_client_t* http, int mode, int ret)
{
	int error_code;

	error_code = last_error();
	if(error_code == HTTP_EINTR)
	{
		return 0;
	}
	else if(error_code == HTTP_EINPROGRESS || error_code == HTTP_EWOULDBLOCK)
	{
		if(socket_select(http, mode, http->timeout) == 0)
		{
			return 0;
		}
	}
	return -1;
}

static int http_read_write(ft_http_client_t* http, const char* data, int len, int read)
{
	int n = 0, r = 0;

	do
	{
		if(http->exit == 1)
		{
			return -1;
		}


		r = read ? recv(http->fd, (char*)data + n, len - n, 0) : send(http->fd, data + n, len - n, 0);
		//CHANGE:
		if(read==0)
		{
			printf("%s",data);
		}	
		
		if(r > 0)
		{
			n += r;
		}
		else if(r == 0)
		{
			return n;
		}
		else
		{
			if(http_check_error(http, read ? kSelectRead : kSelectWrite, r) == 0)
			{
				continue;
			}
			return -1;
		}
	}while(n < len);

	return n;
}

static int http_connect_host(ft_http_client_t* http, const char* url, struct http_parser_url* u)
{
//CHANGE:
printf("url in http_coonect_host=n%s\n",url);
	struct sockaddr_in sin;
	char host[256] = {0};
	int r;
	unsigned short port = 80;

	if(u->field_set & (1 << UF_SCHEMA)){
		 http->proto_type = PROTO_HTTP;
	}

	if(!(u->field_set & (1 << UF_HOST)))
	{
		return ERR_URL_INVALID_HOST;
	}

	if(u->field_set & (1 << UF_PORT))
	{
		port = (unsigned short)atoi(url + u->field_data[UF_PORT].off);
	}
	
	memset(&sin, 0, sizeof(struct sockaddr_in));
	memcpy(host, url + u->field_data[UF_HOST].off, u->field_data[UF_HOST].len);
printf("host=%s\n",host);
	if(host[0] >= '0' && host[0] <= '9')
	{
		printf("****************\n");
		sin.sin_addr.s_addr = /*(unsigned long)*/inet_addr(host);
	}
//#ifdef PLATFORM_LINUX
	else
	{
		struct hostent* he = gethostbyname(host);
		if(he == NULL || he->h_addrtype != AF_INET)
		{
			return ERR_URL_RESOLVED_HOST;
		}
		sin.sin_addr = *((struct in_addr *)he->h_addr_list[0]);
	}
//#endif
	if(sin.sin_addr.s_addr == INADDR_NONE)
	{
		return ERR_URL_RESOLVED_HOST;
	}

	sin.sin_port = htons(port);
printf("port=%d\n",port);
	sin.sin_family = AF_INET;

	close_socket(http->fd);

	http->fd = socket(AF_INET, SOCK_STREAM, 0);

	if(http->fd == HTTP_INVALID_SOCKET)
	{
		return ERR_SOCKET_CREATE;
	}
	
	{
		struct linger linger;
		linger.l_onoff = 1;
		linger.l_linger = 0;  /* 关闭close wait */

		if(setsockopt(http->fd,SOL_SOCKET, SO_LINGER,(const char *) &linger,sizeof(linger)) != 0)
		{
			return ERR_SOCKET_SET_OPT;
		}
		if(socket_noblocking(http->fd, 1) != 0)
		{
			return ERR_SOCKET_NOBLOCKING;
		}
	}

	do
	{
		r = connect(http->fd, (struct sockaddr *)&sin, sizeof(sin));
		printf("r=%d\n",r);
		if(r < 0)
		{
//#ifdef PLATFORM_LINUX
			printf("1\n");
			int error = last_error();
			if(error == HTTP_EINTR)
			{
				printf("2\n");
				continue;
			}
			else if( error == HTTP_EINPROGRESS || error == HTTP_EWOULDBLOCK ||  error == HTTP_EALREADY)
			{
				if(socket_select(http, kSelectWrite, http->timeout) == 0)
				{
					printf("3\n");
					break;
				}
				else
				{
					printf("4\n");
					return ERR_SOCKET_CONNECT_TIMEOUT;
				}
			}
//#endif
			return ERR_SOCKET_CONNECT;
		}
	}while(1);

	return ERR_OK;
}

static int http_internal_sync_request(ft_http_client_t* http, const char* url, 
											  const char* post_data, int post_data_len,
											  const char* user_header, int user_header_len)
{
	http_parser_settings parser_setting;
	struct http_parser parser;
	struct http_parser_url u;
	int r = 0;
	int parsed = 0;
	unsigned short port = 80;

	free_member(http->body);
	free_member(http->header_field);
	free_member(http->header_value);
	close_socket(http->fd);
	
	http->redirect = 0;
	http->body_len = 0;
	http->content_length = 0;
	http->cur_field_size = 0;
	http->cur_value_size = 0;
	http->field_size = 0;
	http->value_size = 0;
	http->parser_statue = 0;
	http->error_code = 0;
	
//CHANGE:
	printf("url:\n%s\n",url);

	if(http->timeout == 0) http->timeout = 15000;

	if( http_parser_parse_url(url, strlen(url), 0, &u) != 0 )
	{
		//CHANGE:
		printf("1");
		return (http->error_code = ERR_URL_INVALID);
	}


	r = http_connect_host(http, url, &u);
		printf("err_code:\n%d\n",r);
	if(r != ERR_OK)
	{
		//CHANGE:
		printf("2");
		return http->error_code = r;
	}
	

#define CHECK(nwirte) \
	if((nwirte) <= 0) { return http->error_code = ERR_SOCKET_WRITE; }

	if(http->method == M_GET)
	{
		CHECK(http_read_write(http, "GET ", 4, 0));
	}
	else if(http->method == M_POST)
	{
		//CHANGE:
		printf("3");
		CHECK(http_read_write(http, "POST ", 5, 0));
	}

	if(u.field_set & (1 << UF_PATH))
	{
		//CHANGE:
		printf("4");
		CHECK(http_read_write(http, url + u.field_data[UF_PATH].off, u.field_data[UF_PATH].len, 0));
	}
	else
	{
		//CHANGE:
		printf("5");
		CHECK(http_read_write(http, "/", 1, 0));
	}

	if(u.field_set & (1 << UF_QUERY))
	{
		CHECK(http_read_write(http, "?", 1, 0));
		CHECK(http_read_write(http, url + u.field_data[UF_QUERY].off, u.field_data[UF_QUERY].len, 0));
	}

	CHECK(http_read_write(http, " HTTP/1.1\r\nHost:", 16, 0));
	CHECK(http_read_write(http, url + u.field_data[UF_HOST].off, u.field_data[UF_HOST].len, 0));
	CHECK(http_read_write(http, CRLF CONNECT_STR ACCEPT_STR  ,
		2 + strlen(CONNECT_STR) + strlen(ACCEPT_STR), 0));

	if(user_header != NULL)
	{
		CHECK(http_read_write(http, user_header, user_header_len, 0));
	}

	if(post_data && post_data_len > 0)
	{
		char len_data[256] = {0};
		int n = sprintf(len_data, "%s:%d\r\n", CONTENT_TYPE_STR CONTENT_LENGTH_STR, post_data_len);
		CHECK(http_read_write(http, len_data, n, 0));
	}

	CHECK(http_read_write(http, CRLF, 2, 0));

	if(post_data && post_data_len > 0)
	{
		CHECK(http_read_write(http, post_data, post_data_len, 0));
	}

	memset(&parser_setting, 0, sizeof(parser_setting));
	parser_setting.on_body = http->download ? on_download_file_cb : on_body_cb;
	parser_setting.on_message_complete = on_message_complete_cb;
	parser_setting.on_header_field = on_header_field_cb;
	parser_setting.on_header_value = on_header_value_cb;
	parser_setting.on_headers_complete = on_headers_complete_cb;
	parser_setting.on_status = on_status_cb;

	http_parser_init(&parser, HTTP_RESPONSE);
	parser.data = http;

	do
	{
		int nread = 0;

		if(http->download == 0 && http->parser_statue == PARSERD_BODY && http->body && http->content_length > 0)
		{
			nread = http_read_write(http, http->body+http->body_len, http->content_length - http->body_len, 1);
			if(nread > 0)
			{
				http->body_len += nread; break;
			}

			//todo: if iss download, use mmap to write data
		}
		else
		{
		//CHANGE:
		printf("6");
			char buf[RECV_BUF_SIZE + 1] = {0};

			nread = http_read_write(http, buf, RECV_BUF_SIZE, 1);

			if(nread > 0)
			{
				parsed = http_parser_execute(&parser, &parser_setting, buf, nread);

				if(http->redirect)
				{
					break;
				}

				if(parsed != nread)
				{
					return http->error_code = ERR_PARSE_REP;
				}
			}
		}

		if(nread == 0)
		{
			break;
		}
		else if(nread < 0)
		{
			return http->error_code = ERR_SOCKET_READ;
		}

	} while (1);

	if(http->redirect == 1)
	{
		return http_internal_sync_request(http, http->redirect_url, post_data, post_data_len, user_header, user_header_len);
	}
	else
	{
		if(http->download)
		{
			if(http->pf) 
			{
				fclose(http->pf);
				http->pf = NULL;
			}
		}
		if(http->body && http->body_len > 0)
		{
			http->body[http->body_len] = '\0';
		}
	}

	return http->error_code;

}

HTTP_API const char* ft_http_sync_request(ft_http_client_t* http, const char* url, http_request_method_e m)
{
	//CHANGES:
	printf("url0:\n%s\n",url);
	cJSON *root = NULL;
	cJSON *test = NULL;
	cJSON *rand = NULL;
	root = cJSON_CreateObject();
	rand = cJSON_CreateObject();
/*
	cJSON_AddStringToObject(root, "method", "OS01");
	cJSON_AddStringToObject(root, "productId", "1553061026000");
	cJSON_AddStringToObject(root, "sn", "123456789011112");
	cJSON_AddStringToObject(root, "imei", "862075031356789");
	cJSON_AddStringToObject(root, "osVersion", "geneva-mp3-nb_V170");
	cJSON_AddNumberToObject(root, "status", 0);
*/
	cJSON_AddNumberToObject(rand, "randomB", -1172028779);
	cJSON_AddStringToObject(root, "carVin", "BNHED7EGFF8A7146");
	cJSON_AddItemToObject(root, "identityAuthInfo", rand);
	char *out = cJSON_Print(root);

	//CHANGE:
	printf("Json:\n%s\n",out);

	if(http == NULL)
	{
		return NULL;
	}

	http->method = m;
	http->download = 0;

	http->error_code = http_internal_sync_request(http, url, (char*)out, strlen(out), 0, 0);
/*
	free(out);
	cJSON_Delete(root);
	root = cJSON_Parse(http->body);
	test = cJSON_GetObjectItem(root, "url");
	printf("url=%s\n", test->valuestring);
	
	cJSON_Delete(root);
*/
	return http->body;
}


HTTP_API const char* ft_http_sync_post_file(ft_http_client_t* http, const char* url, const char* filepath)
{
	static const char* boundary = "-------------------------87142694621188";

	return NULL;
}

HTTP_API int ft_http_sync_download_file(ft_http_client_t* http, const char* url, const char* filepath)
{
	if(http == NULL)
	{
		return -1;
	}

	http->method = M_GET;
	http->download = 1;
	
	free_member(http->filename);
	if(filepath != NULL)
	{
		http->filename = _strdup(filepath);

		if(http->filename == NULL)
		{
			return http->error_code = ERR_OUT_MEMORY;
		}
	}

	if(http_internal_sync_request(http, url, 0, 0, 0, 0) == ERR_OK)
	{
		return ERR_OK;
	}

	return http->error_code;
}

HTTP_API int ft_http_cancel_request(ft_http_client_t* http)
{
	if(http && http->fd != HTTP_INVALID_SOCKET)
	{
		close_socket(http->fd);
	}

	return 0;
}

HTTP_API int ft_http_wait_done(ft_http_client_t* http)
{
	return 0;
}

HTTP_API int ft_http_exit(ft_http_client_t* http)
{
	if(http) http->exit = 1;

	return 0;
}

HTTP_API int ft_http_set_data_recv_cb(ft_http_client_t* http, data_recv_cb_t cb, void* user)
{
	if(http)
	{
		http->user = user;
		http->recv_cb = cb;
	}
	return 0;
}

