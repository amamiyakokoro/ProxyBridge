#include "../src/pb_internal.h"
static int fetches, sizings;
static BOOL grow_once = TRUE, fail_fetch = FALSE;
static DWORD WINAPI test_tcp_table(PVOID buffer, PDWORD size, BOOL order, ULONG family, TCP_TABLE_CLASS kind, ULONG reserved)
{
    DWORD needed = family == AF_INET ? sizeof(MIB_TCPTABLE_OWNER_PID) + sizeof(MIB_TCPROW_OWNER_PID)
                                     : sizeof(MIB_TCP6TABLE_OWNER_PID) + sizeof(MIB_TCP6ROW_OWNER_PID);
    if (!buffer) { sizings++; *size = needed; return ERROR_INSUFFICIENT_BUFFER; }
    fetches++;
    if (fail_fetch) return ERROR_ACCESS_DENIED;
    if (grow_once) { grow_once = FALSE; *size = *size + 8192; return ERROR_INSUFFICIENT_BUFFER; }
    if (*size < needed) { *size = needed; return ERROR_INSUFFICIENT_BUFFER; }
    memset(buffer, 0, needed);
    if (family == AF_INET) {
        MIB_TCPTABLE_OWNER_PID *table = buffer;
        table->dwNumEntries = 2;
        for (int i = 0; i < 2; i++) { table->table[i].dwLocalAddr = inet_addr("192.0.2.1"); table->table[i].dwLocalPort = htons(51000); }
        table->table[1].dwOwningPid = 123;
    } else {
        MIB_TCP6TABLE_OWNER_PID *table = buffer;
        table->dwNumEntries = 2;
        for (int i = 0; i < 2; i++) { table->table[i].ucLocalAddr[15] = 5; table->table[i].dwLocalPort = htons(51000); }
        table->table[1].dwOwningPid = 456;
    }
    return NO_ERROR;
}
#define GetExtendedTcpTable test_tcp_table
#include "../src/pb_process.c"
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); return 1; } } while (0)
int main(void)
{
    UINT32 source = inet_addr("192.0.2.1");
    CHECK(get_process_id_from_connection(source, 51000) == 123);
    CHECK(sizings == 1 && fetches == 2);
    remove_cached_pid(source, 51000, FALSE);
    CHECK(get_process_id_from_connection(source, 51000) == 123);
    CHECK(sizings == 1 && fetches == 3);
    UINT8 source6[16] = {0}; source6[15] = 5;
    CHECK(get_process_id_from_connection_v6(source6, 51000) == 456);
    fail_fetch = TRUE;
    CHECK(get_process_id_from_connection_v6(source6, 51000) == 0);
    free(g_pidtbl_buf); g_pidtbl_buf = NULL; g_pidtbl_cap = 0;
    clear_pid_cache();
    puts("Owner table growth retry, scratch reuse, TIME_WAIT skip and failure checks passed");
    return 0;
}
