#ifndef CR3_WEB_TRANSFER_H
#define CR3_WEB_TRANSFER_H
#include <curl/curl.h>
#include <string>
#include <cstdio>
// One HTTPS transfer, advanced by the UI timer. No worker calls InkView APIs.
class WebTransfer {
    CURL *easy = NULL;
    CURLM *multi = NULL;
    FILE *file = NULL;
    std::string path, text, failure;
    size_t limit = 0;
    curl_off_t bytes = 0, length = 0;
    bool finished = true, success = false, fileOpened = false;
    char details[CURL_ERROR_SIZE] = {};
    static size_t write(char *, size_t, size_t, void *);
    void close();
public:
    WebTransfer() {}
    ~WebTransfer() { close(); }
    WebTransfer(const WebTransfer &) = delete;
    WebTransfer &operator=(const WebTransfer &) = delete;
    bool start(const std::string &url, const std::string &output, size_t maximum);
    bool poll(); // true once completed (including a failure)
    void cancel();
    bool succeeded() const { return success; }
    const std::string &body() const { return text; }
    const std::string &error() const { return failure; }
    curl_off_t received() const { return bytes; }
    curl_off_t total() const { return length; }
};
#endif
