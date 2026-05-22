#include "switchbox/Http.hpp"

#include <fstream>
#include <sstream>

#if defined(SWITCHBOX_USE_CURL) && __has_include(<curl/curl.h>)
#define SWITCHBOX_HAS_CURL 1
#include <curl/curl.h>
#endif

namespace switchbox {
namespace {

bool readFile(const std::string& path, std::string& body, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "failed to open file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    body = ss.str();
    return true;
}

#ifdef SWITCHBOX_HAS_CURL
size_t writeCurlData(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

bool fetchCurl(const std::string& url, std::string& body, std::string& error) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        error = "curl init failed";
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SwitchBox/0.2");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCurlData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    const CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        error = std::string("curl error: ") + curl_easy_strerror(rc);
        curl_easy_cleanup(curl);
        return false;
    }
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    if (code >= 400) {
        error = "http status " + std::to_string(code);
        return false;
    }
    return true;
}
#endif

} // namespace

bool HttpClient::isUrl(const std::string& location) {
    return location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0;
}

bool HttpClient::fetchText(const std::string& location, std::string& body, std::string& error) {
    body.clear();
    error.clear();
    if (!isUrl(location)) {
        const size_t query = location.find('?');
        return readFile(query == std::string::npos ? location : location.substr(0, query), body, error);
    }
#ifdef SWITCHBOX_HAS_CURL
    return fetchCurl(location, body, error);
#else
    error = "network support is not enabled. Build with SWITCHBOX_USE_CURL and link libcurl, or use a local file.";
    return false;
#endif
}

} // namespace switchbox
