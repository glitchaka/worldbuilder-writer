#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <winhttp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "embedded_app.inc"

#define ARCHIVO_PORT 48731
#define REQUEST_BUFFER 131072

static volatile LONG running = 1;
static char window_token[33];

typedef struct WindowSearch {
  HWND found;
} WindowSearch;

static BOOL CALLBACK find_window_callback(HWND window, LPARAM data) {
  if (!IsWindowVisible(window)) return TRUE;
  wchar_t title[512];
  if (!GetWindowTextW(window, title, (int)(sizeof(title) / sizeof(title[0])))) return TRUE;
  if (wcsstr(title, L"El Archivo Neo") != NULL) {
    ((WindowSearch *)data)->found = window;
    return FALSE;
  }
  return TRUE;
}

static HWND find_archive_window(void) {
  WindowSearch search = {0};
  EnumWindows(find_window_callback, (LPARAM)&search);
  return search.found;
}

static int send_all(SOCKET client, const unsigned char *buffer, size_t length) {
  size_t sent = 0;
  while (sent < length) {
    int chunk = (int)((length - sent) > 32768 ? 32768 : (length - sent));
    int result = send(client, (const char *)buffer + sent, chunk, 0);
    if (result == SOCKET_ERROR || result == 0) return 0;
    sent += (size_t)result;
  }
  return 1;
}

static void send_json(SOCKET client, int status, const char *body) {
  char header[512];
  const char *status_text = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 502 ? "Bad Gateway" : "Not Found";
  int header_length = snprintf(
    header,
    sizeof(header),
    "HTTP/1.1 %d %s\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
    status,
    status_text,
    (unsigned int)strlen(body)
  );
  send_all(client, (const unsigned char *)header, (size_t)header_length);
  send_all(client, (const unsigned char *)body, strlen(body));
}

static void send_json_bytes(SOCKET client, int status, const unsigned char *body, size_t body_length) {
  char header[512];
  const char *status_text = status == 200 ? "OK" : "Bad Gateway";
  int header_length = snprintf(
    header,
    sizeof(header),
    "HTTP/1.1 %d %s\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
    status,
    status_text,
    (unsigned int)body_length
  );
  send_all(client, (const unsigned char *)header, (size_t)header_length);
  send_all(client, body, body_length);
}

static void send_empty(SOCKET client) {
  const char *response = "HTTP/1.1 204 No Content\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
  send_all(client, (const unsigned char *)response, strlen(response));
}

static void send_app(SOCKET client, bool head_only) {
  const char *placeholder = "__ARCHIVO_NEO_NATIVE_WINDOW_TOKEN_7E91__";
  const char *position = strstr((const char *)embedded_app_html, placeholder);
  size_t placeholder_length = strlen(placeholder);
  size_t before = position ? (size_t)(position - (const char *)embedded_app_html) : embedded_app_html_len;
  size_t after = position ? embedded_app_html_len - before - placeholder_length : 0;
  size_t content_length = before + (position ? strlen(window_token) : 0) + after;
  char header[512];
  int header_length = snprintf(
    header,
    sizeof(header),
    "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %u\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",
    (unsigned int)content_length
  );
  send_all(client, (const unsigned char *)header, (size_t)header_length);
  if (head_only) return;
  send_all(client, embedded_app_html, before);
  if (position) {
    send_all(client, (const unsigned char *)window_token, strlen(window_token));
    send_all(client, embedded_app_html + before + placeholder_length, after);
  }
}

static void create_window_token(void) {
  unsigned char bytes[16];
  if (BCryptGenRandom(NULL, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
    DWORD fallback = GetTickCount() ^ GetCurrentProcessId();
    for (int index = 0; index < 16; index++) bytes[index] = (unsigned char)((fallback >> ((index % 4) * 8)) ^ (index * 41));
  }
  for (int index = 0; index < 16; index++) snprintf(window_token + (index * 2), 3, "%02x", bytes[index]);
  window_token[32] = '\0';
}

static bool valid_control_request(const char *request) {
  const char *headers = strstr(request, "\r\n");
  return headers != NULL && strstr(headers, window_token) != NULL;
}

static void proxy_proofread(const char *request, SOCKET client) {
  if (!valid_control_request(request)) {
    send_json(client, 403, "{\"error\":\"Solicitud no autorizada.\"}");
    return;
  }
  const char *body = strstr(request, "\r\n\r\n");
  if (!body || !body[4]) {
    send_json(client, 502, "{\"error\":\"No se recibió texto para revisar.\"}");
    return;
  }
  body += 4;
  DWORD body_length = (DWORD)strlen(body);
  HINTERNET session = WinHttpOpen(L"El Archivo Neo/0.2", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  HINTERNET connection = session ? WinHttpConnect(session, L"api.languagetool.org", INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
  HINTERNET remote = connection ? WinHttpOpenRequest(connection, L"POST", L"/v2/check", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
  const wchar_t *headers = L"Content-Type: application/x-www-form-urlencoded\r\nAccept: application/json\r\n";
  BOOL ok = remote && WinHttpSendRequest(remote, headers, (DWORD)-1L, (LPVOID)body, body_length, body_length, 0) && WinHttpReceiveResponse(remote, NULL);
  DWORD status = 0;
  DWORD status_size = sizeof(status);
  if (ok) ok = WinHttpQueryHeaders(remote, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);

  const size_t maximum = 4u * 1024u * 1024u;
  unsigned char *response = ok && status >= 200 && status < 300 ? (unsigned char *)malloc(maximum) : NULL;
  size_t response_length = 0;
  while (response && response_length < maximum) {
    DWORD available = 0;
    DWORD read = 0;
    if (!WinHttpQueryDataAvailable(remote, &available) || available == 0) break;
    DWORD requested = available > maximum - response_length ? (DWORD)(maximum - response_length) : available;
    if (!WinHttpReadData(remote, response + response_length, requested, &read) || read == 0) break;
    response_length += read;
  }

  if (response && response_length > 0) send_json_bytes(client, 200, response, response_length);
  else send_json(client, 502, "{\"error\":\"LanguageTool no respondió.\"}");

  free(response);
  if (remote) WinHttpCloseHandle(remote);
  if (connection) WinHttpCloseHandle(connection);
  if (session) WinHttpCloseHandle(session);
}

static void perform_window_action(const char *request, SOCKET client) {
  if (!valid_control_request(request)) {
    send_json(client, 403, "{\"error\":\"Control de ventana no autorizado.\"}");
    return;
  }

  HWND window = find_archive_window();
  if (strstr(request, "POST /__window/minimize ") == request) {
    send_json(client, 200, "{\"ok\":true}");
    if (window) ShowWindow(window, SW_MINIMIZE);
    return;
  }
  if (strstr(request, "POST /__window/maximize ") == request) {
    bool maximized = false;
    if (window) {
      if (IsZoomed(window)) ShowWindow(window, SW_RESTORE);
      else ShowWindow(window, SW_MAXIMIZE);
      maximized = IsZoomed(window) != 0;
    }
    send_json(client, 200, maximized ? "{\"ok\":true,\"maximized\":true}" : "{\"ok\":true,\"maximized\":false}");
    return;
  }
  if (strstr(request, "POST /__window/close ") == request) {
    send_json(client, 200, "{\"ok\":true}");
    if (window) PostMessageW(window, WM_CLOSE, 0, 0);
    InterlockedExchange(&running, 0);
    return;
  }
  send_json(client, 404, "{\"error\":\"Acción desconocida.\"}");
}

static bool build_edge_profile(wchar_t *profile, size_t profile_count) {
  wchar_t local_app_data[MAX_PATH];
  DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return false;
  wchar_t root[MAX_PATH];
  if (swprintf(root, MAX_PATH, L"%ls\\El Archivo Neo", local_app_data) < 0) return false;
  CreateDirectoryW(root, NULL);
  if (swprintf(profile, profile_count, L"%ls\\Browser", root) < 0) return false;
  CreateDirectoryW(profile, NULL);
  return true;
}

static bool launch_edge(void) {
  wchar_t profile[MAX_PATH];
  wchar_t parameters[2048];
  const wchar_t *url = L"http://127.0.0.1:48731/";
  if (!build_edge_profile(profile, MAX_PATH)) profile[0] = L'\0';
  swprintf(
    parameters,
    sizeof(parameters) / sizeof(parameters[0]),
    L"--kiosk \"%ls\" --edge-kiosk-type=fullscreen --no-first-run --disable-session-crashed-bubble --disable-features=msEdgeSidebarV2 --overscroll-history-navigation=0 --user-data-dir=\"%ls\"",
    url,
    profile
  );

  HINSTANCE launched = ShellExecuteW(NULL, L"open", L"msedge.exe", parameters, NULL, SW_SHOWNORMAL);
  if ((INT_PTR)launched > 32) return true;

  wchar_t program_files[MAX_PATH];
  const wchar_t *variables[] = {L"ProgramFiles(x86)", L"ProgramFiles"};
  for (int index = 0; index < 2; index++) {
    DWORD length = GetEnvironmentVariableW(variables[index], program_files, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) continue;
    wchar_t edge_path[MAX_PATH];
    swprintf(edge_path, MAX_PATH, L"%ls\\Microsoft\\Edge\\Application\\msedge.exe", program_files);
    launched = ShellExecuteW(NULL, L"open", edge_path, parameters, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)launched > 32) return true;
  }
  return false;
}

static SOCKET create_server(void) {
  SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server == INVALID_SOCKET) return INVALID_SOCKET;
  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(ARCHIVO_PORT);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(server, (const struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR || listen(server, 8) == SOCKET_ERROR) {
    closesocket(server);
    return INVALID_SOCKET;
  }
  return server;
}

static int request_content_length(char *request, char *headers_end) {
  char *line = request;
  while (line && line < headers_end) {
    if (_strnicmp(line, "Content-Length:", 15) == 0) return atoi(line + 15);
    line = strstr(line, "\r\n");
    if (line) line += 2;
  }
  return 0;
}

static int receive_request(SOCKET client, char *request, int capacity) {
  int received = 0;
  int expected = -1;
  while (received < capacity - 1) {
    int part = recv(client, request + received, capacity - received - 1, 0);
    if (part <= 0) break;
    received += part;
    request[received] = '\0';
    char *headers_end = strstr(request, "\r\n\r\n");
    if (!headers_end) continue;
    if (expected < 0) {
      int body_length = request_content_length(request, headers_end);
      expected = (int)(headers_end + 4 - request) + body_length;
    }
    if (received >= expected) break;
  }
  request[received] = '\0';
  return received;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show) {
  (void)instance;
  (void)previous;
  (void)command_line;
  (void)show;

  WSADATA winsock;
  if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
    MessageBoxW(NULL, L"No se pudo iniciar el almacenamiento local de la aplicación.", L"El Archivo Neo", MB_ICONERROR | MB_OK);
    return 1;
  }

  SOCKET server = create_server();
  if (server == INVALID_SOCKET) {
    HWND existing = find_archive_window();
    if (existing) {
      ShowWindow(existing, SW_RESTORE);
      SetForegroundWindow(existing);
      WSACleanup();
      return 0;
    }
    MessageBoxW(NULL, L"El puerto local de El Archivo Neo está ocupado. Cierra la otra instancia y vuelve a intentarlo.", L"El Archivo Neo", MB_ICONWARNING | MB_OK);
    WSACleanup();
    return 2;
  }

  create_window_token();
  if (!launch_edge()) {
    closesocket(server);
    WSACleanup();
    MessageBoxW(NULL, L"No se encontró Microsoft Edge. Instálalo o repáralo para abrir esta edición portátil.", L"El Archivo Neo", MB_ICONERROR | MB_OK);
    return 3;
  }

  bool saw_window = false;
  while (InterlockedCompareExchange(&running, 1, 1) == 1) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(server, &read_set);
    struct timeval timeout = {1, 0};
    int ready = select(0, &read_set, NULL, NULL, &timeout);
    if (ready > 0 && FD_ISSET(server, &read_set)) {
      SOCKET client = accept(server, NULL, NULL);
      if (client != INVALID_SOCKET) {
        char request[REQUEST_BUFFER];
        int received = receive_request(client, request, REQUEST_BUFFER);
        if (received > 0) {
          if (strstr(request, "POST /__window/") == request) perform_window_action(request, client);
          else if (strstr(request, "POST /__online/proofread ") == request) proxy_proofread(request, client);
          else if (strstr(request, "GET /favicon.ico ") == request) send_empty(client);
          else if (strstr(request, "HEAD ") == request) send_app(client, true);
          else send_app(client, false);
        }
        shutdown(client, SD_BOTH);
        closesocket(client);
      }
    }

    HWND window = find_archive_window();
    if (window) saw_window = true;
    else if (saw_window) InterlockedExchange(&running, 0);
  }

  closesocket(server);
  WSACleanup();
  return 0;
}
