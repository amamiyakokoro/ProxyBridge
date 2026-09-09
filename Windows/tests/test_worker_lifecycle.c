#include "../src/pb_internal.h"

#include <stdio.h>

#define TEST_TIMEOUT_MS 5000

static void test_log(const char *message)
{
    fprintf(stderr, "%s\n", message);
}

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

static BOOL test_relay_cancellation(void)
{
    SOCKET app = INVALID_SOCKET;
    SOCKET relay_client = INVALID_SOCKET;
    SOCKET relay_proxy = INVALID_SOCKET;
    SOCKET server = INVALID_SOCKET;
    HANDLE thread = NULL;
    TRANSFER_CONFIG *config = NULL;
    char byte = 0;
    BOOL passed = FALSE;

    if (!make_socket_pair(&app, &relay_client) ||
        !make_socket_pair(&relay_proxy, &server))
        goto cleanup;

    shutdown_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (shutdown_event == NULL)
        goto cleanup;
    running = TRUE;

    config = (TRANSFER_CONFIG *)malloc(sizeof(*config));
    if (config == NULL)
        goto cleanup;
    config->from_socket = relay_client;
    config->to_socket = relay_proxy;
    thread = CreateThread(NULL, 0, transfer_handler, config, 0, NULL);
    if (thread == NULL)
        goto cleanup;
    config = NULL;
    relay_client = INVALID_SOCKET;
    relay_proxy = INVALID_SOCKET;

    // Receiving through the pair proves it has been registered and both relay
    // directions are blocked in I/O before cancellation is requested.
    if (send(app, "x", 1, 0) != 1 || recv(server, &byte, 1, 0) != 1 || byte != 'x')
        goto cleanup;

    running = FALSE;
    SetEvent(shutdown_event);
    abort_active_relays();
    passed = WaitForSingleObject(thread, TEST_TIMEOUT_MS) == WAIT_OBJECT_0;

cleanup:
    running = FALSE;
    if (shutdown_event != NULL) SetEvent(shutdown_event);
    abort_active_relays();
    if (app != INVALID_SOCKET) closesocket(app);
    if (relay_client != INVALID_SOCKET) closesocket(relay_client);
    if (relay_proxy != INVALID_SOCKET) closesocket(relay_proxy);
    if (server != INVALID_SOCKET) closesocket(server);
    if (thread != NULL)
    {
        WaitForSingleObject(thread, TEST_TIMEOUT_MS);
        CloseHandle(thread);
    }
    free(config);
    if (shutdown_event != NULL) CloseHandle(shutdown_event);
    shutdown_event = NULL;
    return passed;
}

static BOOL test_cleanup_wakeup(void)
{
    HANDLE thread;
    DWORD started;
    DWORD elapsed;

    shutdown_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (shutdown_event == NULL)
        return FALSE;
    running = TRUE;
    thread = CreateThread(NULL, 0, cleanup_worker, NULL, 0, NULL);
    if (thread == NULL)
    {
        CloseHandle(shutdown_event);
        shutdown_event = NULL;
        running = FALSE;
        return FALSE;
    }

    started = GetTickCount();
    running = FALSE;
    SetEvent(shutdown_event);
    BOOL passed = WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0;
    elapsed = GetTickCount() - started;

    CloseHandle(thread);
    CloseHandle(shutdown_event);
    shutdown_event = NULL;
    return passed && elapsed < 1000;
}

static BOOL test_proxy_readiness(void)
{
    HANDLE thread;
    UINT16 saved_port = g_local_relay_port;
    BOOL passed = FALSE;

    proxy_ready_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (proxy_ready_event == NULL)
        return FALSE;
    InterlockedExchange(&proxy_start_status, 0);
    g_local_relay_port = 0; // Let Windows choose an unused test port.
    running = TRUE;
    thread = CreateThread(NULL, 0, local_proxy_server, NULL, 0, NULL);
    if (thread != NULL)
    {
        passed = WaitForSingleObject(proxy_ready_event, TEST_TIMEOUT_MS) == WAIT_OBJECT_0 &&
                 InterlockedCompareExchange(&proxy_start_status, 0, 0) == 1;
        running = FALSE;
        if (WaitForSingleObject(thread, 2000) != WAIT_OBJECT_0)
            passed = FALSE;
        CloseHandle(thread);
    }

    running = FALSE;
    g_local_relay_port = saved_port;
    CloseHandle(proxy_ready_event);
    proxy_ready_event = NULL;
    return passed;
}

int main(void)
{
    WSADATA winsock;
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
        return 1;

    g_log_callback = test_log;
    BOOL relay_passed = test_relay_cancellation();
    BOOL cleanup_passed = test_cleanup_wakeup();
    BOOL readiness_passed = test_proxy_readiness();
    BOOL passed = relay_passed && cleanup_passed && readiness_passed;
    WSACleanup();

    if (!passed)
    {
        fprintf(stderr, "Worker lifecycle failure: relay=%d cleanup=%d readiness=%d\n",
                relay_passed, cleanup_passed, readiness_passed);
        return 1;
    }
    puts("Worker lifecycle tests passed");
    return 0;
}
