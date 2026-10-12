#include "../src/pb_relay.c"
#define main half_close_scenario
#include "test_tcp_half_close.c"
#undef main

#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); return 1; } } while (0)
static DWORD WINAPI send_bulk(LPVOID argument)
{
    SOCKET socket = *(SOCKET *)argument;
    char buffer[65536];
    for (int i = 0; i < sizeof(buffer); i++) buffer[i] = (char)(i % 251);
    for (int i = 0; i < 64; i++)
        if (send_all(socket, buffer, sizeof(buffer)) != sizeof(buffer)) return 1;
    return shutdown(socket, SD_SEND) == 0 ? 0 : 1;
}

int main(void)
{
    WSADATA wsa;
    CHECK(WSAStartup(MAKEWORD(2,2), &wsa) == 0);
    running = TRUE;
    CHECK(relay_pool_start());
    CHECK(half_close_scenario() == 0);
    CHECK(WaitForSingleObject(g_relay_done, 5000) == WAIT_OBJECT_0);
    enum { CONNECTIONS = 128 };
    SOCKET apps[CONNECTIONS], servers[CONNECTIONS];
    for (int i = 0; i < CONNECTIONS; i++) {
        SOCKET client, proxy;
        CHECK(make_socket_pair(&apps[i], &client));
        CHECK(make_socket_pair(&proxy, &servers[i]));
        CHECK(relay_start(client, proxy));
    }
    CHECK(g_active_relays == CONNECTIONS);
    CHECK(g_relay_nworkers >= 1 && g_relay_nworkers <= 8);
    char bytes[64];
    for (int i = 0; i < CONNECTIONS; i++) {
        CHECK(send_all(apps[i], "request", 7) == 7);
        CHECK(shutdown(apps[i], SD_SEND) == 0);
        CHECK(receive_until_eof(servers[i], bytes, sizeof(bytes)) == 7);
        CHECK(memcmp(bytes, "request", 7) == 0);
        CHECK(send_all(servers[i], "response", 8) == 8);
        CHECK(shutdown(servers[i], SD_SEND) == 0);
        CHECK(receive_until_eof(apps[i], bytes, sizeof(bytes)) == 8);
        CHECK(memcmp(bytes, "response", 8) == 0);
        closesocket(apps[i]); closesocket(servers[i]);
    }
    CHECK(WaitForSingleObject(g_relay_done, 5000) == WAIT_OBJECT_0);
    CHECK(g_active_relays == 0);
    // Streaming data exercises full buffers, partial sends and integrity across completions.
    SOCKET bulk_app, bulk_client, bulk_proxy, bulk_server;
    CHECK(make_socket_pair(&bulk_app, &bulk_client));
    CHECK(make_socket_pair(&bulk_proxy, &bulk_server));
    CHECK(relay_start(bulk_client, bulk_proxy));
    HANDLE sender = CreateThread(NULL, 0, send_bulk, &bulk_app, 0, NULL);
    CHECK(sender != NULL);
    char bulk_bytes[65536];
    unsigned total = 0;
    int length;
    while ((length = recv(bulk_server, bulk_bytes, sizeof(bulk_bytes), 0)) > 0) {
        for (int i = 0; i < length; i++) CHECK(bulk_bytes[i] == (char)((total + i) % 65536 % 251));
        total += length;
    }
    CHECK(length == 0 && total == 64 * 65536);
    CHECK(WaitForSingleObject(sender, 5000) == WAIT_OBJECT_0);
    DWORD sender_result; CHECK(GetExitCodeThread(sender, &sender_result) && sender_result == 0);
    CloseHandle(sender);
    CHECK(send_all(bulk_server, "verified", 8) == 8 && shutdown(bulk_server, SD_SEND) == 0);
    CHECK(receive_until_eof(bulk_app, bytes, sizeof(bytes)) == 8 && memcmp(bytes, "verified", 8) == 0);
    closesocket(bulk_app); closesocket(bulk_server);
    CHECK(WaitForSingleObject(g_relay_done, 5000) == WAIT_OBJECT_0);
    // Stop while idle asynchronous receives are pending; every completion must drain.
    SOCKET client, proxy, app, server;
    CHECK(make_socket_pair(&app, &client));
    CHECK(make_socket_pair(&proxy, &server));
    CHECK(relay_start(client, proxy));
    running = FALSE;
    relay_pool_stop();
    CHECK(g_active_relays == 0 && g_iocp == NULL);
    closesocket(app); closesocket(server);
    WSACleanup();
    puts("IOCP pool: 128 parallel relays, half-close integrity and idle stop passed");
    return 0;
}
