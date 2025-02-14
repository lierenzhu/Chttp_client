#ifndef _HTTP_H_
#define _HTTP_H_

#define HTTP_API

#ifdef __cplusplus
extern "C" {
#endif

typedef enum http_request_method_e 
{ 
	M_GET = 0, 
	M_POST, 
	M_HEAD 
} http_request_method_e;

enum http_error_e
{
	ERR_OK = 0,

	ERR_INVALID_PARAM,
	ERR_OUT_MEMORY,
	ERR_OPEN_FILE,

	ERR_PARSE_REP,

	ERR_URL_INVALID,
	ERR_URL_INVALID_PROTO,
	ERR_URL_INVALID_HOST,
	ERR_URL_INVALID_IP,
	ERR_URL_RESOLVED_HOST,

	ERR_SOCKET_CREATE,
	ERR_SOCKET_SET_OPT,
	ERR_SOCKET_NOBLOCKING,
	ERR_SOCKET_CONNECT,
	ERR_SOCKET_CONNECT_TIMEOUT,
	ERR_SOCKET_SELECT,
	ERR_SOCKET_WRITE,
	ERR_SOCKET_READ,
	ERR_SOCKET_TIMEOUT,
	ERR_SOCKET_CLOSED,
	ERR_SOCKET_GET_OPT,
};

struct ft_http_client_t;
typedef struct ft_http_client_t ft_http_client_t;

typedef int (*data_recv_cb_t)( ft_http_client_t* http, const char* data, int size, int total, void* user);


HTTP_API int ft_http_init();

HTTP_API void ft_http_deinit();


HTTP_API ft_http_client_t* ft_http_new();

HTTP_API void ft_http_destroy(ft_http_client_t* http);

HTTP_API int ft_http_get_error_code(ft_http_client_t* http);

HTTP_API int ft_http_get_status_code(ft_http_client_t* http);

HTTP_API int ft_http_set_timeout(ft_http_client_t* http, int timeout);

HTTP_API const char* ft_http_sync_request(ft_http_client_t* http, const char* url, http_request_method_e m_);

HTTP_API int ft_http_sync_download_file( ft_http_client_t* http, const char* url, const char* filepath);

HTTP_API int ft_http_cancel_request(ft_http_client_t* http);

HTTP_API int ft_http_wait_done(ft_http_client_t* http);

HTTP_API int ft_http_set_data_recv_cb(ft_http_client_t* http, data_recv_cb_t cb, void* user);

HTTP_API int ft_http_exit(ft_http_client_t* http);

HTTP_API const char* ft_http_sync_post_file(ft_http_client_t* http, const char* url, const char* filepath);



#ifdef __cplusplus
}
#endif


#endif

