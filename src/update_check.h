#pragma once

#include <winhttp.h>
#include <string>
#include <fstream>
#include <filesystem>
#pragma comment(lib, "winhttp.lib")


static int CompareVersions(const char *a, const char *b) {
  if (!a || !b) return 0;
  if (*a == 'v' || *a == 'V') a++;
  if (*b == 'v' || *b == 'V') b++;

  int a1 = 0, a2 = 0, a3 = 0;
  int b1 = 0, b2 = 0, b3 = 0;
  char trailing = 0;
  if (sscanf(a, "%d.%d.%d%c", &a1, &a2, &a3, &trailing) != 3 ||
      sscanf(b, "%d.%d.%d%c", &b1, &b2, &b3, &trailing) != 3 ||
      a1 < 0 || a2 < 0 || a3 < 0 || b1 < 0 || b2 < 0 || b3 < 0)
    return 0;

  if (a1 != b1) return a1 < b1 ? -1 : 1;
  if (a2 != b2) return a2 < b2 ? -1 : 1;
  if (a3 != b3) return a3 < b3 ? -1 : 1;
  return 0;
}

static std::string HttpGet(const wchar_t *host, const wchar_t *path,
                           int timeout_ms = 5000,
                           const wchar_t *extraHeaders = nullptr) {
  std::string result;

  HINTERNET hSession = WinHttpOpen(
      L"EFF-UpdateCheck/1.0",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) return result;

  WinHttpSetTimeouts(hSession, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

  HINTERNET hConnect = WinHttpConnect(hSession, host,
                                      INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return result;
  }

  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"GET", path, NULL,
      WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
  }

  if (extraHeaders) {
    WinHttpAddRequestHeaders(hRequest, extraHeaders, (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD);
  }

  BOOL ok = WinHttpSendRequest(hRequest,
                                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  if (!ok) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
  }

  ok = WinHttpReceiveResponse(hRequest, NULL);
  if (!ok) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
  }

  DWORD statusCode = 0;
  DWORD statusSize = sizeof(statusCode);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX,
                      &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
  if (statusCode != 200) {
    Log("[UPDATE] HTTP %d from %S%S", statusCode, host, path);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
  }

  DWORD bytesAvailable = 0;
  DWORD bytesRead = 0;
  char buf[4096];
  while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) &&
         bytesAvailable > 0) {
    DWORD toRead = (bytesAvailable < sizeof(buf)) ? bytesAvailable : sizeof(buf);
    if (WinHttpReadData(hRequest, buf, toRead, &bytesRead) && bytesRead > 0) {
      result.append(buf, bytesRead);
    }
    if (result.size() > 65536) { result.clear(); break; }
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return result;
}

static std::string JsonExtractString(const std::string &json, const char *key) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);

  size_t pos = json.find(pattern);
  if (pos == std::string::npos) return "";

  pos = json.find(':', pos + strlen(pattern));
  if (pos == std::string::npos) return "";

  pos = json.find('"', pos + 1);
  if (pos == std::string::npos) return "";
  pos++; 

  std::string value;
  for (size_t i = pos; i < json.size(); i++) {
    if (json[i] == '\\' && i + 1 < json.size()) {
      char next = json[i + 1];
      if (next == '"') { value += '"'; i++; }
      else if (next == 'n') { value += '\n'; i++; }
      else if (next == 'r') { value += '\r'; i++; }
      else if (next == 't') { value += '\t'; i++; }
      else if (next == '\\') { value += '\\'; i++; }
      else { value += json[i]; }
    } else if (json[i] == '"') {
      break; 
    } else {
      value += json[i];
    }
  }
  return value;
}

struct EiemUpdateSnapshot {
  bool checking = false, available = false, latest = false, failed = false;
  std::string version, url;
};

static SRWLOCK s_eiemUpdateLock = SRWLOCK_INIT;
static volatile LONG s_eiemUpdateRunning = 0;
static constexpr const wchar_t *kEiemIgnoredReleasePath =
    L"plugin\\eff-update-state.txt";

static EiemUpdateSnapshot EiemGetUpdateSnapshot() {
  AcquireSRWLockShared(&s_eiemUpdateLock);
  EiemUpdateSnapshot result;
  result.checking = g_updateChecking;
  result.available = g_updateAvailable;
  result.latest = g_updateIsLatest;
  result.failed = g_updateCheckFailed;
  result.version = g_latestVersion;
  result.url = g_updateUrl;
  ReleaseSRWLockShared(&s_eiemUpdateLock);
  return result;
}

static std::string EiemReadIgnoredRelease() {
  std::ifstream input{std::filesystem::path(kEiemIgnoredReleasePath)};
  std::string tag;
  std::getline(input, tag);
  return tag;
}

static bool EiemIgnoreRelease(const std::string &version) {
  if (version.empty() || version.size() > 48) return false;
  for (unsigned char c : version)
    if (!(c >= '0' && c <= '9') && c != '.') return false;
  CreateDirectoryW(L"plugin", nullptr);
  HANDLE file = CreateFileW(kEiemIgnoredReleasePath, GENERIC_WRITE, FILE_SHARE_READ,
                            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  bool ok = WriteFile(file, version.data(), (DWORD)version.size(), &written, nullptr) &&
            written == version.size();
  CloseHandle(file);
  if (ok) {
    AcquireSRWLockExclusive(&s_eiemUpdateLock);
    g_updateAvailable = false;
    ReleaseSRWLockExclusive(&s_eiemUpdateLock);
  }
  return ok;
}

static void CheckForUpdates(bool forceIgnored = false) {
  const std::string body = HttpGet(
      L"api.github.com", L"/repos/ssice-a/EIEM/releases/latest", 3000,
      L"Accept: application/vnd.github+json\r\nUser-Agent: EFF-UpdateCheck/1.0\r\n");
  std::string tag = JsonExtractString(body, "tag_name");
  std::string url = JsonExtractString(body, "html_url");
  const std::string prefix = "https://github.com/ssice-a/EIEM/releases/";
  int major = 0, minor = 0, patch = 0;
  char extra = 0;
  const char *number = tag.c_str();
  if (*number == 'v' || *number == 'V') ++number;
  const bool valid = !body.empty() && url.compare(0, prefix.size(), prefix) == 0 &&
          url.size() <= 511 && sscanf(number, "%d.%d.%d%c",
                                      &major, &minor, &patch, &extra) == 3 &&
          major >= 0 && minor >= 0 && patch >= 0;
  std::string version = valid ? number : "";
  const bool ignored = valid && !forceIgnored && EiemReadIgnoredRelease() == version;

  AcquireSRWLockExclusive(&s_eiemUpdateLock);
  g_updateAvailable = valid && !ignored && CompareVersions(EIEM_VERSION, version.c_str()) < 0;
  g_updateIsLatest = valid && CompareVersions(EIEM_VERSION, version.c_str()) >= 0;
  g_updateCheckFailed = !valid;
  g_updateDismissed = false;
  g_updateChangelog[0] = '\0';
  strncpy(g_latestVersion, version.c_str(), sizeof(g_latestVersion) - 1);
  g_latestVersion[sizeof(g_latestVersion) - 1] = '\0';
  strncpy(g_updateUrl, valid ? url.c_str() : "", sizeof(g_updateUrl) - 1);
  g_updateUrl[sizeof(g_updateUrl) - 1] = '\0';
  g_updateResultTime = GetTickCount();
  g_updateChecking = false;
  ReleaseSRWLockExclusive(&s_eiemUpdateLock);
  Log("[UPDATE] check %s latest=%s%s", valid ? "ok" : "failed",
      version.c_str(), ignored ? " ignored" : "");
}

static DWORD WINAPI UpdateCheckThread(LPVOID parameter) {
  if (!g_shutdownRequested) CheckForUpdates(parameter != nullptr);
  InterlockedExchange(&s_eiemUpdateRunning, 0);
  return 0;
}

static bool EiemRequestUpdateCheck(bool forceIgnored = false) {
  if (!forceIgnored) {
    const auto lastCheck = g_updateResultTime;
    if (lastCheck && GetTickCount() - lastCheck < 15 * 60 * 1000) return false;
  }
  if (InterlockedCompareExchange(&s_eiemUpdateRunning, 1, 0) != 0) return false;
  AcquireSRWLockExclusive(&s_eiemUpdateLock);
  g_updateChecking = true;
  g_updateAvailable = false;
  g_updateCheckFailed = g_updateIsLatest = false;
  ReleaseSRWLockExclusive(&s_eiemUpdateLock);
  if (g_updateThread) { CloseHandle(g_updateThread); g_updateThread = nullptr; }
  g_updateThread = CreateThread(nullptr, 0, UpdateCheckThread,
                                forceIgnored ? (LPVOID)1 : nullptr, 0, nullptr);
  if (g_updateThread) return true;
  AcquireSRWLockExclusive(&s_eiemUpdateLock);
  g_updateChecking = false;
  g_updateCheckFailed = true;
  ReleaseSRWLockExclusive(&s_eiemUpdateLock);
  InterlockedExchange(&s_eiemUpdateRunning, 0);
  return false;
}
