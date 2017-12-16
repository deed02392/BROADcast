// =============================================================================
// BROADcast
//
// Force IPv4 UDP broadcast on all network interfaces in Windows 7 and later.
//
// Copyright Kristian Garnét et al.
// -----------------------------------------------------------------------------

// Windows 7+ ------------------------------------------------------------------

#ifdef __MINGW32__
  #define _WIN32_WINNT 0x0601
#endif

// Unicode ---------------------------------------------------------------------

#ifdef UNICODE
  #ifndef _UNICODE
    #define _UNICODE
  #endif
#endif

// Includes --------------------------------------------------------------------

#include <Winsock2.h>
#include <Windows.h>
#include <Mswsock.h>
#include <Ws2tcpip.h>
#include <Iphlpapi.h>

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>

// Constants -------------------------------------------------------------------

#ifndef VERSION
  #define VERSION "1.0"
#endif

// -----------------------------------------------------------------------------

#if (BUF_SIZE == 0)
  #define BUF_SIZE 4096u
#endif

#if (BUF_SIZE < 256)
  #error "Invalid definition of `BUF_SIZE`"
#endif

// -----------------------------------------------------------------------------

#define IP_HEADER_SIZE 20u
#define IP_ADDR_SRC_POS 12u
#define IP_ADDR_DST_POS 16u

#define UDP_HEADER_SIZE 8u
#define UDP_CHECKSUM_POS 6u

#define METRIC_CHANGE_TRIES_MAX 3u

// Macros ----------------------------------------------------------------------

#ifdef UNICODE
  #define tchar_t wchar_t
  #define tstricmp _wcsicmp
  #define tprintf wprintf
  #define tputs _putws
#else
  #define tchar_t char
  #define tstricmp _stricmp
  #define tprintf printf
  #define tputs puts
#endif

// Variables -------------------------------------------------------------------

static SOCKET sock_listen;

static ULONG addr_localhost;
static ULONG addr_broadcast;

static BOOL diag;

// Utilities -------------------------------------------------------------------

#ifndef UNICODE
static char* utf16_to_ansi (const wchar_t* wstr)
{
  char* str;

  int strsz = WideCharToMultiByte (CP_THREAD_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
  if (!strsz) return NULL;

  str = malloc (strsz);
  if (str == NULL) return NULL;

  if (!WideCharToMultiByte (CP_THREAD_ACP, 0, wstr, -1, str, strsz, NULL, NULL))
  {
    free (str);
    return NULL;
  }

  return str;
}
#endif

// -----------------------------------------------------------------------------

static void udp_chksum (unsigned char* payload, size_t payload_sz
, DWORD addr_src, DWORD addr_dst)
{
  WORD* r = (WORD*)payload;
  size_t sz = payload_sz;

  WORD* src = (WORD*)&addr_src;
  WORD* dst = (WORD*)&addr_dst;

  DWORD chksum = 0;

  // Reset to zero
  *(WORD*)(payload + UDP_CHECKSUM_POS) = 0;

  // Compute data
  while (sz > 1u)
  {
    chksum += *r++;
    if (chksum & 0x80000000u) chksum = (chksum & 0xFFFFu) + (chksum >> 16);
    sz -= 2u;
  }

  // Last byte of data
  if (sz) chksum += *r;

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
  while (chksum >> 16) chksum = (chksum & 0xFFFFu) + (chksum >> 16);

  // Write the result
  *(WORD*)(payload + UDP_CHECKSUM_POS) = (WORD)(~chksum);
}

// Functions -------------------------------------------------------------------

static int metric_update (const tchar_t* iface, BOOL manual)
{
  ULONG metric = 0;

  // Obtain the list of Ethernet & Wi-Fi adapters
  PIP_ADAPTER_ADDRESSES addresses = NULL, runner;
  DWORD addresses_sz = 0, tries = 0, result;

  do
  {
    result = GetAdaptersAddresses (AF_UNSPEC, GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST
    | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, addresses, &addresses_sz);

    if (result == ERROR_BUFFER_OVERFLOW)
    {
      free (addresses);
      addresses = malloc (addresses_sz);

      if (addresses == NULL) return FALSE;
    }
    else if (result != NO_ERROR)
    {
      free (addresses);
      return FALSE;
    }

    tries++;
  }
  while ((result == ERROR_BUFFER_OVERFLOW) && (tries < METRIC_CHANGE_TRIES_MAX));

  if (tries == METRIC_CHANGE_TRIES_MAX)
  {
    free (addresses);
    return FALSE;
  }

  // Look up the requested network interface
  if (manual)
  {
    runner = addresses;

    while (runner != NULL)
    {
      tchar_t* iface_name;

      if ((runner->IfType != IF_TYPE_ETHERNET_CSMACD)
      && (runner->IfType != IF_TYPE_IEEE80211))
      {
        runner = runner->Next;
        continue;
      }

#ifdef UNICODE
      iface_name = runner->FriendlyName;
#else
      iface_name = utf16_to_ansi (runner->FriendlyName);

      if (iface_name == NULL)
      {
        free (addresses);
        return FALSE;
      }
#endif

      if (tstricmp (iface, iface_name) == 0)
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
      return FALSE;
    }
  }

  // Update metric values
  runner = addresses;

  while (runner != NULL)
  {
    tchar_t* iface_name;

    if ((runner->IfType != IF_TYPE_ETHERNET_CSMACD)
    && (runner->IfType != IF_TYPE_IEEE80211))
    {
      runner = runner->Next;
      continue;
    }

#ifdef UNICODE
    iface_name = runner->FriendlyName;
#else
    iface_name = utf16_to_ansi (runner->FriendlyName);

    if (iface_name == NULL)
    {
      free (addresses);
      return FALSE;
    }
#endif

    if (tstricmp (iface, iface_name) != 0)
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

  return TRUE;
}

// -----------------------------------------------------------------------------

static void broadcast_quit (int err, const tchar_t* msg)
{
  shutdown (sock_listen, SD_RECEIVE);
  closesocket (sock_listen);

  WSACleanup();

  if (msg != NULL) tputs (msg);

  exit (err);
}

// -----------------------------------------------------------------------------

static void broadcast_loop()
{
  unsigned char buf[BUF_SIZE];

  ULONG addr_src, addr_dst, addr_route, addr_src_new, i;

  SOCKET sock_src_new;
  SOCKADDR_IN sa_addr_src_new = {0}, sa_addr_dst = {0};
  sockaddr_gen sa_addr_broadcast = {0}, sa_addr_route = {0};

  WSABUF wsa_buf = {0};
  DWORD read_sz = 0, code = 0, flags = 0;

  PMIB_IPFORWARDTABLE fwd_table = NULL;
  ULONG fwd_table_sz = 0;

  unsigned char opt_broadcast = 1u;

  // Initialize destination address buffer
  sa_addr_dst.sin_family = AF_INET;
  sa_addr_dst.sin_addr.s_addr = addr_broadcast;

  // Initialize broadcast address buffer
  sa_addr_broadcast.Address.sa_family = AF_INET;
  sa_addr_broadcast.AddressIn.sin_addr.s_addr = addr_broadcast;

  // Until interrupted
  while (TRUE)
  {
    // Initialize the buffer
    wsa_buf.buf = buf;
    wsa_buf.len = sizeof (buf);

    // Reset
    read_sz = 0;
    flags = 0;

    // We are listening on a raw socket
    while ((code = WSARecv (sock_listen, &wsa_buf, 1u, &read_sz, &flags, NULL, NULL)) == 0)
    {
      // Wait until we have a complete UDP datagram with header
      if (read_sz < (IP_HEADER_SIZE + UDP_HEADER_SIZE)) continue;

      // Get the packet addresses
      addr_src = *(ULONG*)(buf + IP_ADDR_SRC_POS);
      addr_dst = *(ULONG*)(buf + IP_ADDR_DST_POS);

      // Find out the preferred broadcast route
      if (WSAIoctl (sock_listen, SIO_ROUTING_INTERFACE_QUERY, &sa_addr_broadcast
      , sizeof (sa_addr_broadcast), &sa_addr_route, sizeof (sa_addr_route)
      , &i, NULL, NULL) == SOCKET_ERROR)
      {
        if (!((WSAGetLastError() == WSAENETUNREACH) || (WSAGetLastError() == WSAEHOSTUNREACH)
        || (WSAGetLastError() == WSAENETDOWN))) broadcast_quit (EXIT_FAILURE
        , TEXT("Couldn't get the preferred broadcast route.\n"));
      }

      addr_route = sa_addr_route.AddressIn.sin_addr.s_addr;

      // Diagnostics
      if (diag)
      {
        tprintf (TEXT("Source: %u.%u.%u.%u; Destination: %u.%u.%u.%u; Preferred: %u.%u.%u.%u; Size: %u;\n")
        , addr_src & 0xFFu, (addr_src >> 8) & 0xFFu, (addr_src >> 16) & 0xFFu, (addr_src >> 24) & 0xFFu
        , addr_dst & 0xFFu, (addr_dst >> 8) & 0xFFu, (addr_dst >> 16) & 0xFFu, (addr_dst >> 24) & 0xFFu
        , addr_route & 0xFFu, (addr_route >> 8) & 0xFFu, (addr_route >> 16) & 0xFFu, (addr_route >> 24) & 0xFFu
        , (unsigned)read_sz);
      }

      // Got broadcast packet from the preferred route?
      if ((addr_src == addr_route) && (addr_dst == addr_broadcast)) break;
    }

    if (code != 0) broadcast_quit (EXIT_FAILURE, TEXT("Error listening on the broadcast socket.\n"));

    // Prepare buffer for send operation
    wsa_buf.buf = buf + IP_HEADER_SIZE;
    wsa_buf.len = read_sz - IP_HEADER_SIZE;

    // Get the forwarding table
    i = 0;

    while ((code = GetIpForwardTable (fwd_table, &fwd_table_sz, FALSE)) != NO_ERROR)
    {
      i++;

      if ((code == ERROR_INSUFFICIENT_BUFFER) && (i < 3u))
      {
        fwd_table = realloc (fwd_table, fwd_table_sz);
        continue;
      }

      broadcast_quit (EXIT_FAILURE, TEXT("Error getting the forwarding table.\n"));
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

      // Initialize new source socket, by default in blocking mode:
      // http://msdn.microsoft.com/en-us/library/windows/desktop/ms738573(v=vs.85).aspx
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
      udp_chksum ((unsigned char*)wsa_buf.buf, wsa_buf.len
      , sa_addr_src_new.sin_addr.s_addr, sa_addr_dst.sin_addr.s_addr);

      if (WSASendTo (sock_src_new, &wsa_buf, 1u, &code, 0
      , (SOCKADDR*)&sa_addr_dst, sizeof (sa_addr_dst)
      , NULL, NULL) != 0)
      {
        if (WSAGetLastError() != WSAEWOULDBLOCK) broadcast_quit (EXIT_FAILURE
        , TEXT("Error relaying data.\n"));

        closesocket (sock_src_new);

        continue;
      }

      // Diagnostics
      if (diag)
      {
        tprintf (TEXT("Relayed: %u.%u.%u.%u;\n"), addr_src_new & 0xFFu
        , (addr_src_new >> 8) & 0xFFu, (addr_src_new >> 16) & 0xFFu
        , (addr_src_new >> 24) & 0xFFu);
      }

      closesocket (sock_src_new);
    }
  }
}

// -----------------------------------------------------------------------------

static void signal_handler (int signum)
{
  if (diag) tprintf (TEXT("Exiting on signal %d.\n"), signum);

  broadcast_quit ((signum == SIGINT) ? EXIT_SUCCESS : EXIT_FAILURE, NULL);
}

// -----------------------------------------------------------------------------

static void broadcast_start (void)
{
  WORD wsa_ver = MAKEWORD (2, 2);
  WSADATA wsa_data = {0};

  SOCKADDR_IN sa_localhost = {0};

  // Initialize Winsock
  if (WSAStartup (wsa_ver, &wsa_data) != 0) broadcast_quit (EXIT_FAILURE
  , TEXT("Error initializing Winsock.\n"));

  // Initialize addresses
  addr_localhost = inet_addr ("127.0.0.1");
  addr_broadcast = inet_addr ("255.255.255.255");

  // Bind to localhost
  sock_listen = WSASocket (AF_INET, SOCK_RAW, IPPROTO_UDP, NULL, 0, 0);

  if (sock_listen == INVALID_SOCKET) broadcast_quit (EXIT_FAILURE
  , TEXT("Error creating the listening socket.\n"));

  sa_localhost.sin_family = AF_INET;
  sa_localhost.sin_addr.s_addr = addr_localhost;

  if (bind (sock_listen, (SOCKADDR*)&sa_localhost, sizeof (sa_localhost))
  == SOCKET_ERROR) broadcast_quit (EXIT_FAILURE
  , TEXT("Error binding on the listening socket.\n"));

  // Handle termination cleanly
  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);
  signal (SIGABRT, signal_handler);

  // Enter the broadcast loop
  broadcast_loop();
}

// =============================================================================

#ifdef UNICODE
int wmain (int argc, wchar_t** argv)
#else
int main (int argc, char** argv)
#endif
{
  // Windows 7+ is recommended
  OSVERSIONINFOEX osvi;
  DWORDLONG dwlConditionMask = 0;

  // Initialize the `OSVERSIONINFOEX` structure
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
    tputs (TEXT("This program isn't guaranteed to work on Windows Vista and earlier.\n"));
  }

  // Parse command line arguments (order matters)
  tchar_t* app = argv[0];

  if ((argc == 4) || (argc == 3))
  {
    argc--;
    argv++;

    if (tstricmp (TEXT("/i"), argv[0]) == 0)
    {
      argc--;
      argv++;

      BOOL ret = metric_update (argv[0], ((argc == 2)
      && (tstricmp (TEXT("/m"), argv[1]) == 0)) ? TRUE : FALSE);

      return ret ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  }

  if ((argc == 3) || (argc == 2))
  {
    argc--;
    argv++;

    if (tstricmp (TEXT("/?"), argv[0]) == 0)
    {
usage:
      tprintf (TEXT("BROADcast") TEXT(" ") TEXT(VERSION)
TEXT("\n\n")
TEXT("Force IPv4 UDP broadcast on all network interfaces.\n")
TEXT("This program must be run with administrator privileges.\n")
TEXT("\n")
TEXT("%s /i <interface> [/m]\n")
TEXT("%s /b [/d]\n"), app, app);

      return EXIT_FAILURE;
    }

    if (tstricmp (TEXT("/b"), argv[0]) == 0)
    {
      diag = ((argc == 2) && (tstricmp (TEXT("/d"), argv[1]) == 0));
      broadcast_start();
    }
    else goto usage;
  }
  else goto usage;

  return EXIT_SUCCESS;
}
