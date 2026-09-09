#include "../src/pb_internal.h"

#include <stdio.h>

#define TEST_TIMEOUT_MS 5000

static void set_test_timeout(SOCKET sock)
{
    DWORD timeout = TEST_TIMEOUT_MS;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
}

static BOOL make_socket_pair(SOCKET *client, SOCKET *server)
{
    SOCKET listener = INVALID_SOCKET;
    struct sockaddr_in address;
    int address_len = sizeof(address);

    *client = INVALID_SOCKET;
    *server = INVALID_SOCKET;
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
        goto fail;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR ||
        listen(listener, 1) == SOCKET_ERROR ||
        getsockname(listener, (struct sockaddr *)&address, &address_len) == SOCKET_ERROR)
        goto fail;

    *client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*client == INVALID_SOCKET ||
        connect(*client, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR)
        goto fail;

    *server = accept(listener, NULL, NULL);
    if (*server == INVALID_SOCKET)
        goto fail;

    closesocket(listener);
    set_test_timeout(*client);
    set_test_timeout(*server);
    return TRUE;

fail:
    if (listener != INVALID_SOCKET) closesocket(listener);
    if (*client != INVALID_SOCKET) closesocket(*client);
    if (*server != INVALID_SOCKET) closesocket(*server);
    *client = INVALID_SOCKET;
    *server = INVALID_SOCKET;
    return FALSE;
}

static int receive_until_eof(SOCKET sock, char *buffer, int capacity)
{
    int total = 0;
    for (;;)
    {
        int received = recv(sock, buffer + total, capacity - total, 0);
        if (received == 0)
            return total;
        if (received == SOCKET_ERROR || received > capacity - total)
            return -1;
        total += received;
        if (total == capacity)
            return -1;
    }
}

int main(void)
{
    static const char request[] = "request-before-half-close";
    static const char response[] = "response-after-half-close";
    WSADATA winsock;
    SOCKET app = INVALID_SOCKET;
    SOCKET relay_client = INVALID_SOCKET;
    SOCKET relay_proxy = INVALID_SOCKET;
    SOCKET server = INVALID_SOCKET;
    HANDLE relay_thread = NULL;
    TRANSFER_CONFIG *config = NULL;
    char received[128] = {0};
    int result = 1;

    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
        return 1;
    if (!make_socket_pair(&app, &relay_client) ||
        !make_socket_pair(&relay_proxy, &server))
        goto cleanup;

    config = (TRANSFER_CONFIG *)malloc(sizeof(*config));
    if (config == NULL)
        goto cleanup;
    config->from_socket = relay_client;
    config->to_socket = relay_proxy;
    relay_thread = CreateThread(NULL, 0, transfer_handler, config, 0, NULL);
    if (relay_thread == NULL)
        goto cleanup;
    config = NULL;
    relay_client = INVALID_SOCKET;
    relay_proxy = INVALID_SOCKET;

    if (send_all(app, request, (int)strlen(request)) == SOCKET_ERROR ||
        shutdown(app, SD_SEND) == SOCKET_ERROR)
        goto cleanup;

    // Do not send the response until the relayed FIN arrives. The old SD_BOTH
    // behavior aborted the reverse direction at exactly this point.
    int request_len = receive_until_eof(server, received, sizeof(received));
    if (request_len != (int)strlen(request) || memcmp(received, request, request_len) != 0)
        goto cleanup;
    if (send_all(server, response, (int)strlen(response)) == SOCKET_ERROR ||
        shutdown(server, SD_SEND) == SOCKET_ERROR)
        goto cleanup;

    memset(received, 0, sizeof(received));
    int response_len = receive_until_eof(app, received, sizeof(received));
    if (response_len != (int)strlen(response) || memcmp(received, response, response_len) != 0)
        goto cleanup;
    if (WaitForSingleObject(relay_thread, TEST_TIMEOUT_MS) != WAIT_OBJECT_0)
        goto cleanup;

    result = 0;
    puts("TCP half-close relay test passed");

cleanup:
    free(config);
    if (relay_thread != NULL) CloseHandle(relay_thread);
    if (app != INVALID_SOCKET) closesocket(app);
    if (relay_client != INVALID_SOCKET) closesocket(relay_client);
    if (relay_proxy != INVALID_SOCKET) closesocket(relay_proxy);
    if (server != INVALID_SOCKET) closesocket(server);
    WSACleanup();
    return result;
}
