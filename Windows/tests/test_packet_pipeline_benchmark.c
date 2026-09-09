#include "../src/pb_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IO_BUFFER_SIZE (128 * 1024)
#define DEFAULT_BYTES_MB 256
#define DEFAULT_PING_COUNT 2000
#define BENCH_TIMEOUT_MS 30000

typedef struct BENCH_SERVER {
    HANDLE ready_event;
    HANDLE stop_event;
    UINT16 port;
    unsigned long long byte_count;
    UINT ping_count;
    volatile LONG result;
} BENCH_SERVER;

typedef struct CPU_TIME {
    unsigned long long kernel;
    unsigned long long user;
} CPU_TIME;

static void benchmark_log(const char *message)
{
    fprintf(stderr, "%s\n", message);
}

static double elapsed_seconds(LARGE_INTEGER start, LARGE_INTEGER end,
                              LARGE_INTEGER frequency)
{
    return (double)(end.QuadPart - start.QuadPart) /
           (double)frequency.QuadPart;
}

static CPU_TIME read_cpu_time(void)
{
    FILETIME creation;
    FILETIME exit;
    FILETIME kernel;
    FILETIME user;
    ULARGE_INTEGER kernel_value;
    ULARGE_INTEGER user_value;
    CPU_TIME result = {0, 0};

    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
        return result;
    kernel_value.LowPart = kernel.dwLowDateTime;
    kernel_value.HighPart = kernel.dwHighDateTime;
    user_value.LowPart = user.dwLowDateTime;
    user_value.HighPart = user.dwHighDateTime;
    result.kernel = kernel_value.QuadPart;
    result.user = user_value.QuadPart;
    return result;
}

static double cpu_seconds(CPU_TIME start, CPU_TIME end)
{
    unsigned long long start_ticks = start.kernel + start.user;
    unsigned long long end_ticks = end.kernel + end.user;
    return (double)(end_ticks - start_ticks) / 10000000.0;
}

static void configure_socket(SOCKET sock)
{
    BOOL enabled = TRUE;
    DWORD timeout = BENCH_TIMEOUT_MS;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
               (const char *)&enabled, sizeof(enabled));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               (const char *)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
               (const char *)&timeout, sizeof(timeout));
}

static BOOL send_exact(SOCKET sock, const char *buffer,
                       unsigned long long byte_count)
{
    unsigned long long sent_total = 0;
    while (sent_total < byte_count)
    {
        unsigned long long remaining = byte_count - sent_total;
        int chunk = remaining > IO_BUFFER_SIZE ? IO_BUFFER_SIZE : (int)remaining;
        int sent = send(sock, buffer, chunk, 0);
        if (sent == SOCKET_ERROR || sent == 0)
            return FALSE;
        sent_total += (unsigned int)sent;
    }
    return TRUE;
}

static BOOL receive_exact(SOCKET sock, char *buffer,
                          unsigned long long byte_count)
{
    unsigned long long received_total = 0;
    while (received_total < byte_count)
    {
        unsigned long long remaining = byte_count - received_total;
        int chunk = remaining > IO_BUFFER_SIZE ? IO_BUFFER_SIZE : (int)remaining;
        int received = recv(sock, buffer, chunk, 0);
        if (received == SOCKET_ERROR || received == 0)
            return FALSE;
        received_total += (unsigned int)received;
    }
    return TRUE;
}

static SOCKET accept_configured(SOCKET listener, HANDLE stop_event)
{
    SOCKET client;
    for (;;)
    {
        fd_set read_fds;
        struct timeval timeout = {0, 100000};
        if (WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0)
            return INVALID_SOCKET;
        FD_ZERO(&read_fds);
        FD_SET(listener, &read_fds);
        int ready = select(0, &read_fds, NULL, NULL, &timeout);
        if (ready == SOCKET_ERROR)
            return INVALID_SOCKET;
        if (ready > 0)
            break;
    }
    client = accept(listener, NULL, NULL);
    if (client != INVALID_SOCKET)
        configure_socket(client);
    return client;
}

static DWORD WINAPI benchmark_server(LPVOID arg)
{
    BENCH_SERVER *server = (BENCH_SERVER *)arg;
    SOCKET listener = INVALID_SOCKET;
    SOCKET throughput = INVALID_SOCKET;
    SOCKET ping = INVALID_SOCKET;
    struct sockaddr_in address;
    int address_len = sizeof(address);
    char *buffer = NULL;
    char byte = 0;

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
        goto done;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR ||
        listen(listener, 2) == SOCKET_ERROR ||
        getsockname(listener, (struct sockaddr *)&address, &address_len) == SOCKET_ERROR)
        goto done;

    server->port = ntohs(address.sin_port);
    SetEvent(server->ready_event);

    buffer = (char *)malloc(IO_BUFFER_SIZE);
    if (buffer == NULL)
        goto done;
    memset(buffer, 0x5a, IO_BUFFER_SIZE);

    throughput = accept_configured(listener, server->stop_event);
    if (throughput == INVALID_SOCKET ||
        !receive_exact(throughput, buffer, server->byte_count) ||
        !send_exact(throughput, buffer, server->byte_count))
        goto done;
    shutdown(throughput, SD_BOTH);
    closesocket(throughput);
    throughput = INVALID_SOCKET;

    ping = accept_configured(listener, server->stop_event);
    if (ping == INVALID_SOCKET)
        goto done;
    for (UINT i = 0; i < server->ping_count; i++)
    {
        if (!receive_exact(ping, &byte, 1) || !send_exact(ping, &byte, 1))
            goto done;
    }

    InterlockedExchange(&server->result, 1);

done:
    if (server->port == 0)
        SetEvent(server->ready_event);
    free(buffer);
    if (throughput != INVALID_SOCKET) closesocket(throughput);
    if (ping != INVALID_SOCKET) closesocket(ping);
    if (listener != INVALID_SOCKET) closesocket(listener);
    if (InterlockedCompareExchange(&server->result, 0, 0) != 1)
        InterlockedExchange(&server->result, -1);
    return 0;
}

static SOCKET connect_to_server(UINT16 port)
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in address;
    if (sock == INVALID_SOCKET)
        return INVALID_SOCKET;
    configure_socket(sock);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (connect(sock, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR)
    {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

static int compare_double(const void *left, const void *right)
{
    double a = *(const double *)left;
    double b = *(const double *)right;
    return (a > b) - (a < b);
}

static double percentile(const double *values, UINT count, double quantile)
{
    UINT index = (UINT)((double)(count - 1) * quantile + 0.5);
    return values[index];
}

static BOOL run_benchmark(unsigned long long byte_count, UINT ping_count,
                          double *upload_mbps, double *download_mbps,
                          double *cpu_core_percent, double *p50_us,
                          double *p95_us, double *p99_us)
{
    BENCH_SERVER server;
    HANDLE thread = NULL;
    SOCKET throughput = INVALID_SOCKET;
    SOCKET ping = INVALID_SOCKET;
    char *buffer = NULL;
    double *latencies = NULL;
    LARGE_INTEGER frequency;
    LARGE_INTEGER upload_start;
    LARGE_INTEGER upload_end;
    LARGE_INTEGER download_end;
    LARGE_INTEGER ping_start;
    LARGE_INTEGER ping_end;
    CPU_TIME cpu_start;
    CPU_TIME cpu_end;
    BOOL passed = FALSE;

    memset(&server, 0, sizeof(server));
    server.byte_count = byte_count;
    server.ping_count = ping_count;
    server.ready_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    server.stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (server.ready_event == NULL || server.stop_event == NULL)
        goto done;
    thread = CreateThread(NULL, 0, benchmark_server, &server, 0, NULL);
    if (thread == NULL ||
        WaitForSingleObject(server.ready_event, BENCH_TIMEOUT_MS) != WAIT_OBJECT_0 ||
        server.port == 0)
        goto done;

    buffer = (char *)malloc(IO_BUFFER_SIZE);
    latencies = (double *)malloc(sizeof(double) * ping_count);
    if (buffer == NULL || latencies == NULL)
        goto done;
    memset(buffer, 0xa5, IO_BUFFER_SIZE);

    throughput = connect_to_server(server.port);
    if (throughput == INVALID_SOCKET)
        goto done;
    QueryPerformanceFrequency(&frequency);
    cpu_start = read_cpu_time();
    QueryPerformanceCounter(&upload_start);
    if (!send_exact(throughput, buffer, byte_count))
        goto done;
    QueryPerformanceCounter(&upload_end);
    if (!receive_exact(throughput, buffer, byte_count))
        goto done;
    QueryPerformanceCounter(&download_end);
    cpu_end = read_cpu_time();
    closesocket(throughput);
    throughput = INVALID_SOCKET;

    ping = connect_to_server(server.port);
    if (ping == INVALID_SOCKET)
        goto done;
    for (UINT i = 0; i < ping_count; i++)
    {
        QueryPerformanceCounter(&ping_start);
        if (!send_exact(ping, buffer, 1) || !receive_exact(ping, buffer, 1))
            goto done;
        QueryPerformanceCounter(&ping_end);
        latencies[i] = elapsed_seconds(ping_start, ping_end, frequency) * 1000000.0;
    }
    shutdown(ping, SD_BOTH);
    closesocket(ping);
    ping = INVALID_SOCKET;

    if (WaitForSingleObject(thread, BENCH_TIMEOUT_MS) != WAIT_OBJECT_0 ||
        InterlockedCompareExchange(&server.result, 0, 0) != 1)
        goto done;

    qsort(latencies, ping_count, sizeof(double), compare_double);
    *upload_mbps = ((double)byte_count * 8.0 / 1000000.0) /
                   elapsed_seconds(upload_start, upload_end, frequency);
    *download_mbps = ((double)byte_count * 8.0 / 1000000.0) /
                     elapsed_seconds(upload_end, download_end, frequency);
    *cpu_core_percent = cpu_seconds(cpu_start, cpu_end) /
                        elapsed_seconds(upload_start, download_end, frequency) * 100.0;
    *p50_us = percentile(latencies, ping_count, 0.50);
    *p95_us = percentile(latencies, ping_count, 0.95);
    *p99_us = percentile(latencies, ping_count, 0.99);
    passed = TRUE;

done:
    if (throughput != INVALID_SOCKET) closesocket(throughput);
    if (ping != INVALID_SOCKET) closesocket(ping);
    if (thread != NULL)
    {
        if (!passed) SetEvent(server.stop_event);
        WaitForSingleObject(thread, BENCH_TIMEOUT_MS);
        CloseHandle(thread);
    }
    if (server.ready_event != NULL) CloseHandle(server.ready_event);
    if (server.stop_event != NULL) CloseHandle(server.stop_event);
    free(buffer);
    free(latencies);
    return passed;
}

static void show_usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s [--windivert] [--bytes-mb N] [--pings N] [--output FILE]\n"
        "  Default mode is a direct loopback baseline. --windivert starts\n"
        "  ProxyBridge with no rules to measure packet capture/reinjection.\n",
        program);
}

int main(int argc, char **argv)
{
    BOOL use_windivert = FALSE;
    const char *output_path = NULL;
    unsigned long bytes_mb = DEFAULT_BYTES_MB;
    unsigned long pings = DEFAULT_PING_COUNT;
    WSADATA wsa;
    double upload_mbps;
    double download_mbps;
    double cpu_core_percent;
    double p50_us;
    double p95_us;
    double p99_us;
    BOOL started = FALSE;
    int result = 1;
    char json[1024];

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--windivert") == 0)
            use_windivert = TRUE;
        else if (strcmp(argv[i], "--bytes-mb") == 0 && i + 1 < argc)
            bytes_mb = strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--pings") == 0 && i + 1 < argc)
            pings = strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            output_path = argv[++i];
        else
        {
            show_usage(argv[0]);
            return 2;
        }
    }
    if (bytes_mb == 0 || pings == 0)
    {
        show_usage(argv[0]);
        return 2;
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;
    if (use_windivert)
    {
        g_local_relay_port = 0; // Avoid conflicting with an installed/running build.
        ProxyBridge_SetLogCallback(benchmark_log);
        started = ProxyBridge_Start();
        if (!started)
        {
            fprintf(stderr,
                "Unable to start WinDivert. Run this benchmark from an elevated "
                "Administrator terminal and keep WinDivert.dll/WinDivert64.sys "
                "beside the executable.\n");
            goto done;
        }
    }

    if (!run_benchmark((unsigned long long)bytes_mb * 1024ULL * 1024ULL,
                       (UINT)pings, &upload_mbps, &download_mbps,
                       &cpu_core_percent, &p50_us, &p95_us, &p99_us))
    {
        fprintf(stderr, "Packet pipeline benchmark failed (%d)\n", WSAGetLastError());
        goto done;
    }

    snprintf(json, sizeof(json),
             "{\"mode\":\"%s\",\"batch\":%d,\"bytes_mb\":%lu,"
             "\"pings\":%lu,\"upload_mbps\":%.3f,\"download_mbps\":%.3f,"
             "\"cpu_core_percent\":%.3f,\"rtt_p50_us\":%.3f,"
             "\"rtt_p95_us\":%.3f,\"rtt_p99_us\":%.3f}",
             use_windivert ? "windivert" : "direct", PACKET_BATCH_SIZE,
             bytes_mb, pings, upload_mbps, download_mbps, cpu_core_percent,
             p50_us, p95_us, p99_us);
    puts(json);
    if (output_path != NULL)
    {
        FILE *output = NULL;
        if (fopen_s(&output, output_path, "w") != 0 || output == NULL)
        {
            fprintf(stderr, "Unable to write benchmark result to %s\n", output_path);
            goto done;
        }
        fprintf(output, "%s\n", json);
        fclose(output);
    }
    result = 0;

done:
    if (started) ProxyBridge_Stop();
    WSACleanup();
    return result;
}
