// =============================================================================
// BROADcast
//
// Force UDP (IPv4) broadcast on all network interfaces in Windows.
//
// Copyright Kristian Garnét et al.
// -----------------------------------------------------------------------------

#ifdef __MINGW32__
  #define _WIN32_WINNT 0x0600
#endif

#ifndef UNICODE
  #ifdef _UNICODE
    #define UNICODE
  #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

#include <Winsock2.h>
#include <Windows.h>
#include <Mswsock.h>
#include <Ws2tcpip.h>
#include <Iphlpapi.h>

// -----------------------------------------------------------------------------

#ifndef VERSION
  #define VERSION "1.0"
#endif

#define RCV_BUF_SZ 4096

#define IP_HEADER_SIZE 20
#define IP_SRCADDR_POS 12
#define IP_DSTADDR_POS 16

#define UDP_HEADER_SIZE 8
#define UDP_CHECKSUM_POS 6

#define METRIC_CHANGE_MAX_TRIES 3

#ifdef UNICODE
  #define rchar wchar_t
  #define rstricmp _wcsicmp
  #define rputs _putws
  #define rprintf wprintf
#else
  #define rchar char
  #define rstricmp _stricmp
  #define rputs puts
  #define rprintf printf
#endif

// -----------------------------------------------------------------------------

static SOCKET sock_listen;

static ULONG addr_localhost;
static ULONG addr_broadcast;

static int debug = 0;

#ifndef UNICODE
// -----------------------------------------------------------------------------

static char* utf16_to_utf8 (wchar_t* wstr)
{
  char* str;

  int str_sz = WideCharToMultiByte (CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
  if (!str_sz) return NULL;

  str = (char*)malloc (str_sz * sizeof (*str));
  if (str == NULL) return NULL;

  if (!WideCharToMultiByte (CP_UTF8, 0, wstr, -1, str, str_sz, NULL, NULL)) return NULL;

  return str;
}
#endif

// -----------------------------------------------------------------------------

static int metric_update (rchar* iface, int manual)
{
  ULONG metric = 0;

  // Obtain Ethernet & Wi-Fi adapter list
  PIP_ADAPTER_ADDRESSES addresses = NULL, runner;
  DWORD addresses_sz = 0, tries = 0, result;

  do
  {
    result = GetAdaptersAddresses (AF_UNSPEC, GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST
    | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, addresses, &addresses_sz);

    if (result == ERROR_BUFFER_OVERFLOW)
    {
      free (addresses);
      addresses = (PIP_ADAPTER_ADDRESSES)malloc (addresses_sz);

      if (addresses == NULL) return 1;
    }
    else if (result != NO_ERROR)
    {
      free (addresses);
      return 1;
    }

    tries++;
  }
  while ((result == ERROR_BUFFER_OVERFLOW) && (tries < METRIC_CHANGE_MAX_TRIES));

  if (tries == METRIC_CHANGE_MAX_TRIES)
  {
    free (addresses);
    return 1;
  }

  // Look up for requested network interface
  if (manual)
  {
    runner = addresses;

    while (runner != NULL)
    {
      rchar* iface_name;

      if (runner->IfType != IF_TYPE_ETHERNET_CSMACD
      && runner->IfType != IF_TYPE_IEEE80211)
      {
        runner = runner->Next;
        continue;
      }

#ifdef UNICODE
      iface_name = runner->FriendlyName;
#else
      iface_name = utf16_to_utf8 (runner->FriendlyName);

      if (iface_name == NULL)
      {
        free (addresses);
        return 1;
      }
#endif

      if (rstricmp (iface, iface_name) == 0)
      {
        // Remember
        metric = runner->Ipv4Metric;

#ifndef UNICODE
        free (iface_name);
#endif

        break;
      }

#ifndef UNICODE
      free (iface_name);
#endif

      runner = runner->Next;
    }

    // Couldn't find
    if (metric == 0)
    {
      free (addresses);
      return 1;
    }
  }

  // Update metric values
  runner = addresses;

  while (runner != NULL)
  {
    rchar* iface_name;

    if (runner->IfType != IF_TYPE_ETHERNET_CSMACD
    && runner->IfType != IF_TYPE_IEEE80211)
    {
      runner = runner->Next;
      continue;
    }

#ifdef UNICODE
    iface_name = runner->FriendlyName;
#else
    iface_name = utf16_to_utf8 (runner->FriendlyName);

    if (iface_name == NULL)
    {
      free (addresses);
      return 1;
    }
#endif

    if (rstricmp (iface, iface_name) != 0)
    {
      MIB_IPINTERFACE_ROW iface_row;

      InitializeIpInterfaceEntry (&iface_row);

      iface_row.InterfaceLuid = runner->Luid;
      iface_row.Family = AF_INET;

      if (manual)
      {
        iface_row.UseAutomaticMetric = FALSE;
        iface_row.Metric = runner->Ipv4Metric + metric;
      }
      else iface_row.UseAutomaticMetric = TRUE;

      SetIpInterfaceEntry (&iface_row);
    }

#ifndef UNICODE
    free (iface_name);
#endif

    runner = runner->Next;
  }

  free (addresses);

  return 0;
}

// -----------------------------------------------------------------------------

static void udp_chksum (char* payload, size_t payload_sz, DWORD addr_src, DWORD addr_dst)
{
  WORD* runner = (WORD*)payload;
  size_t sz = payload_sz;

  WORD* src = (WORD*)&addr_src;
  WORD* dst = (WORD*)&addr_dst;

  DWORD chksum = 0;

  // Reset to zero
  *(WORD*)(payload + UDP_CHECKSUM_POS) = 0;

  // Compute data
  while (sz > 1)
  {
    chksum += *runner++;
    if (chksum & 0x80000000) chksum = (chksum & 0xFFFF) + (chksum >> 16);
    sz -= 2;
  }

  // Last byte of data
  if (sz & 1) chksum += *(uint8_t*)runner;

  // Compute source address
  chksum += *src++;
  chksum += *src;

  // Compute destination address
  chksum += *dst++;
  chksum += *dst;

  // Compute protocol and size
  chksum += htons (IPPROTO_UDP);
  chksum += htons (payload_sz);

  // Compute overflow
  while (chksum >> 16) chksum = (chksum & 0xFFFF) + (chksum >> 16);

  *(WORD*)(payload + UDP_CHECKSUM_POS) = (WORD)(~chksum);
}

// -----------------------------------------------------------------------------

static void broadcast_quit (int err, rchar* msg)
{
  shutdown (sock_listen, SD_RECEIVE);
  closesocket (sock_listen);

  WSACleanup();

  if (msg != NULL) rputs (msg);

  exit (err);
}

static void broadcast_loop()
{
  ULONG addr_src, addr_dst, addr_route, addr_src_new, i;
  SOCKADDR_IN sa_addr_src_new = {0}, sa_addr_dst = {0};
  sockaddr_gen sa_addr_broadcast = {0}, sa_addr_route = {0};
  SOCKET sock_src_new;

  char buf[RCV_BUF_SZ];
  WSABUF wsa_buf = {0};
  DWORD read_sz = 0, code = 0, flags = 0;

  PMIB_IPFORWARDTABLE fwd_table = NULL;
  ULONG fwd_table_sz = 0;

  char opt_broadcast = 1;

  // Initialize destination address
  sa_addr_dst.sin_family = AF_INET;
  sa_addr_dst.sin_addr.s_addr = addr_broadcast;

  // Initialize broadcast address
  sa_addr_broadcast.Address.sa_family = AF_INET;
  sa_addr_broadcast.AddressIn.sin_addr.s_addr = addr_broadcast;

  // Until interrupted
  while (1)
  {
    // Prepare buffer for read operation
    wsa_buf.buf = buf;
    wsa_buf.len = sizeof (buf) / (sizeof (char));

    // Reset
    read_sz = 0;
    flags = 0;

    // We are listening on a raw socket
    while ((code = WSARecv (sock_listen, &wsa_buf, 1, &read_sz, &flags, NULL, NULL)) == 0)
    {
      // Wait until we have complete UDP datagram with header
      if (read_sz < IP_HEADER_SIZE + UDP_HEADER_SIZE) continue;

      // Get packet addresses
      addr_src = *(ULONG*)(buf + IP_SRCADDR_POS);
      addr_dst = *(ULONG*)(buf + IP_DSTADDR_POS);

      // Find out preferred broadcast route
      if (WSAIoctl (sock_listen, SIO_ROUTING_INTERFACE_QUERY, &sa_addr_broadcast, sizeof (sa_addr_broadcast)
      , &sa_addr_route, sizeof (sa_addr_route), &i, NULL, NULL) == SOCKET_ERROR)
      {
        if (!(WSAGetLastError() == WSAENETUNREACH || WSAGetLastError() == WSAEHOSTUNREACH
        || WSAGetLastError() == WSAENETDOWN)) broadcast_quit (1
        , TEXT("Error: couldn't get preferred broadcast route.\n"));
      }

      addr_route = sa_addr_route.AddressIn.sin_addr.s_addr;

      // Show them
      if (debug)
      {
        rprintf (TEXT("Source: %u.%u.%u.%u; Destination: %u.%u.%u.%u; Preferred: %u.%u.%u.%u; Size: %u;\n")
        , (uint8_t)((addr_src) & 0xFF), (uint8_t)((addr_src >> 8) & 0xFF), (uint8_t)((addr_src >> 16) & 0xFF), (uint8_t)((addr_src >> 24) & 0xFF)
        , (uint8_t)((addr_dst) & 0xFF), (uint8_t)((addr_dst >> 8) & 0xFF), (uint8_t)((addr_dst >> 16) & 0xFF), (uint8_t)((addr_dst >> 24) & 0xFF)
        , (uint8_t)((addr_route) & 0xFF), (uint8_t)((addr_route >> 8) & 0xFF), (uint8_t)((addr_route >> 16) & 0xFF), (uint8_t)((addr_route >> 24) & 0xFF)
        , read_sz);
      }

      // Got broadcast packet from the preferred route?
      if (addr_src == addr_route && addr_dst == addr_broadcast) break;
    }

    if (code != 0) broadcast_quit (1, TEXT("Error listening on broadcast socket.\n"));

    // Prepare buffer for send operation
    wsa_buf.buf = buf + IP_HEADER_SIZE;
    wsa_buf.len = read_sz - IP_HEADER_SIZE;

    // Get forwarding table
    i = 0;

    while ((code = GetIpForwardTable (fwd_table, &fwd_table_sz, FALSE)) != NO_ERROR)
    {
      i++;

      if (code == ERROR_INSUFFICIENT_BUFFER && i < 3)
      {
        fwd_table = (PMIB_IPFORWARDTABLE)realloc (fwd_table, fwd_table_sz);
        continue;
      }

      broadcast_quit (1, TEXT("Error getting forwarding table.\n"));
    }

    // Find addresses to relay from
    for (i = 0; i < fwd_table->dwNumEntries; i++)
    {
      // Only local routes with final destination
      if (fwd_table->table[i].dwForwardType != MIB_IPROUTE_TYPE_DIRECT) continue;
      // Netmask must be 255.255.255.255
      if (fwd_table->table[i].dwForwardMask != ULONG_MAX) continue;
      // Destination must be 255.255.255.255
      if (fwd_table->table[i].dwForwardDest != addr_broadcast) continue;
      // Local address must not be 0.0.0.0
      if (fwd_table->table[i].dwForwardNextHop == 0) continue;
      // Local address must not be 127.0.0.1
      if (fwd_table->table[i].dwForwardNextHop == addr_localhost) continue;
      // Local address must not be preferred route
      if (fwd_table->table[i].dwForwardNextHop == addr_src) continue;

      addr_src_new = fwd_table->table[i].dwForwardNextHop;

      // Initialize new source socket (by default in blocking mode:
      // http://msdn.microsoft.com/en-us/library/windows/desktop/ms738573(v=vs.85).aspx)
      sock_src_new = WSASocket (AF_INET, SOCK_RAW, IPPROTO_UDP, NULL, 0, 0);

      if (sock_src_new == INVALID_SOCKET)
      {
        closesocket (sock_src_new);
        continue;
      }

      // Bind it to the next interface and send broadcast packet from it
      sa_addr_src_new.sin_family = AF_INET;
      sa_addr_src_new.sin_addr.s_addr = addr_src_new;

      if (bind (sock_src_new, (SOCKADDR*)&sa_addr_src_new
      , sizeof (sa_addr_src_new)) == SOCKET_ERROR)
      {
        closesocket (sock_src_new);
        continue;
      }

      if (setsockopt (sock_src_new, SOL_SOCKET, SO_BROADCAST
      , &opt_broadcast, sizeof (opt_broadcast)) == SOCKET_ERROR)
      {
        closesocket (sock_src_new);
        continue;
      }

      // Recompute the UDP header checksum
      udp_chksum (wsa_buf.buf, wsa_buf.len, sa_addr_src_new.sin_addr.s_addr, sa_addr_dst.sin_addr.s_addr);

      if (WSASendTo (sock_src_new, &wsa_buf, 1, &code, 0
      , (SOCKADDR*)&sa_addr_dst, sizeof (sa_addr_dst)
      , NULL, NULL) != 0)
      {
        if (WSAGetLastError() != WSAEWOULDBLOCK) broadcast_quit (1, TEXT("Error relaying data.\n"));
        closesocket (sock_src_new);

        continue;
      }

      if (debug)
      {
        rprintf (TEXT("Relayed: %u.%u.%u.%u;\n"), (uint8_t)((addr_src_new) & 0xFF)
        , (uint8_t)((addr_src_new >> 8) & 0xFF), (uint8_t)((addr_src_new >> 16) & 0xFF)
        , (uint8_t)((addr_src_new >> 24) & 0xFF));
      }

      closesocket (sock_src_new);
    }
  }
}

static void signal_handler (int signum)
{
  if (debug) rprintf (TEXT("Exiting on signal %d.\n"), signum);

  broadcast_quit (0, NULL);
}

static void broadcast_start (int dbg)
{
  WORD wsa_ver = MAKEWORD (2, 2);
  WSADATA wsa_data = {0};

  SOCKADDR_IN sa_localhost = {0};

  debug = dbg;

  // Initialize Winsock
  if (WSAStartup (wsa_ver, &wsa_data) != 0) broadcast_quit (1
  , TEXT("Error Initializing Winsock.\n"));

  // Initialize addresses
  addr_localhost = inet_addr ("127.0.0.1");
  addr_broadcast = inet_addr ("255.255.255.255");

  // Bind to localhost
  sock_listen = WSASocket (AF_INET, SOCK_RAW, IPPROTO_UDP, NULL, 0, 0);
  if (sock_listen == INVALID_SOCKET) broadcast_quit (1
  , TEXT("Error creating listening socket.\n"));

  sa_localhost.sin_family = AF_INET;
  sa_localhost.sin_addr.s_addr = addr_localhost;
  if (bind (sock_listen, (SOCKADDR*)&sa_localhost, sizeof (sa_localhost))
  == SOCKET_ERROR) broadcast_quit (1
  , TEXT("Error binding on listening socket.\n"));

  // Handle termination cleanly
  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);
  signal (SIGABRT, signal_handler);

  // Enter the broadcast loop
  broadcast_loop();
}

// -----------------------------------------------------------------------------

#ifdef UNICODE
int wmain (int argc, wchar_t** argv)
#else
int main (int argc, char** argv)
#endif
{
  // Windows 7+ is recommended
  OSVERSIONINFOEX osvi;
  DWORDLONG dwlConditionMask = 0;

  // Initialize the OSVERSIONINFOEX structure
  ZeroMemory (&osvi, sizeof (OSVERSIONINFOEX));

  osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEX);
  osvi.dwMajorVersion = 6;
  osvi.dwMinorVersion = 1;

  // Initialize the condition mask
  VER_SET_CONDITION (dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION (dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

  // Perform the version test
  if (VerifyVersionInfo (&osvi, VER_MAJORVERSION | VER_MINORVERSION
  | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR, dwlConditionMask) == 0)
  {
    rputs (TEXT("This program isn't guaranteed to work on Windows Vista and older.\n"));
  }

  // Parse the command line. Order of parameters matters
  if (argc == 4 || argc == 3)
  {
    argc--;
    argv++;

    if (rstricmp (TEXT("/i"), *argv) == 0)
    {
      argc--;
      argv++;

      return metric_update (*argv, (argc == 2
      && rstricmp (TEXT("/m"), *(argv + 1)) == 0) ? 1 : 0);
    }
  }

  if (argc == 3 || argc == 2)
  {
    argc--;
    argv++;

    if (rstricmp (TEXT("/?"), *argv) == 0)
    {
help:
      rputs (TEXT("BROADcast ")TEXT(VERSION)TEXT("\n\n")
TEXT("Force IPv4 UDP broadcast on all network interfaces.\n\n")
TEXT("Usage:\n")
TEXT("BROADcast.exe /i <interface> [/m]\n")
TEXT("BROADcast.exe /b [/d]\n")
TEXT("(Order of parameters matters.)\n\n")
TEXT("Copyright Kristian Garnet et al.\n\n")
TEXT("https://github.com/garnetius/BROADcast"));
      exit (0);
    }

    if (rstricmp (TEXT("/b"), *argv) == 0) broadcast_start ((argc == 2
    && rstricmp (TEXT("/d"), *(argv + 1)) == 0) ? 1 : 0);
    else goto help;
  }
  else goto help;

  return 1;
}
