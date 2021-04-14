/*
 * @Description: 
 * @Author: lierenzhu
 * @Date: 2021-04-12 09:21:40
 * @LastEditors: lierenzhu
 * @LastEditTime: 2021-04-14 14:22:44
 * @FilePath: /Chttp_client/main.c
 */
#include "http.h"
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

void print_json(cJSON *root)
{
    int i = 0;
    for (i = 0; i < cJSON_GetArraySize(root); i++)
    {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (item->type == cJSON_Object)
        {
            print_json(item);
        }
        else
        {
            printf("%s->", item->string);
            printf("%s\n", cJSON_Print(item));
        }
    }
}

int main()
{

    cJSON *root = NULL;
    cJSON *data = NULL;
    cJSON *handShakeInfo = NULL;
    cJSON *identityAuthInfo = NULL;
    cJSON *item = NULL;

    ft_http_client_t *http = 0;

    ft_http_init();

    http = ft_http_new();

    char url[128];
    scanf("%s", &url);
    const char *body = NULL;
    body = ft_http_sync_request(http, url, M_POST);
    // printf("\nft_http get:%s\n", body);
    if (http)
    {
        ft_http_destroy(http);
    }
    ft_http_deinit();

    printf("\n%s\n", body);
    char result2[2048];
    strcpy(result2, body);
    printf("\n%d\n", strcmp(result2, body));
    root = cJSON_Parse(result2);

    if (!root)
    {
        printf("Error before: [%s]\n", cJSON_GetErrorPtr());
    }
    else
    {
        printf("%s\n", "有格式的方式打印Json:");
        printf("%s\n\n", cJSON_Print(root));
        printf("%s\n", "无格式方式打印json：");
        printf("%s\n\n", cJSON_PrintUnformatted(root));

        printf("%s\n", "一步一步的获取name 键值对:");
        printf("%s\n", "获取data下的cjson对象:");
        data = cJSON_GetObjectItem(root, "data"); //
        printf("%s\n", cJSON_Print(data));
        printf("%s\n", "获取handShakeInfo下的cjson对象");
        handShakeInfo = cJSON_GetObjectItem(data, "handShakeInfo");
        printf("%s\n", cJSON_Print(handShakeInfo));
        printf("%s\n", "获取identityAuthInfo下的cjson对象");
        identityAuthInfo = cJSON_GetObjectItem(data, "identityAuthInfo");
        printf("%s\n", cJSON_Print(identityAuthInfo));

        item = cJSON_GetObjectItem(identityAuthInfo, "serverVinCode");

        printf("%s:", item->string); //看一下cjson对象的结构体中这两个成员的意思
        printf("%s\n", item->valuestring);

        printf("\n%s\n", "打印json所有最内层键值对:");
        print_json(root);
    }

    return 0;
}
