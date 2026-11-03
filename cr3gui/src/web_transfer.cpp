#include "web_transfer.h"
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <limits>
#include <unistd.h>
size_t WebTransfer::write(char *data, size_t size, size_t count, void *context) {
    WebTransfer *self = static_cast<WebTransfer *>(context);
    if (size && count > std::numeric_limits<size_t>::max() / size) return 0;
    size_t n = size * count;
    if (n > self->limit - static_cast<size_t>(self->bytes)) {
        self->failure = "The server response exceeds the permitted size."; return 0;
    }
    if (self->file) {
        size_t written = fwrite(data, 1, n, self->file);
        self->bytes += written;
        if (written != n) self->failure = std::string("Could not save the download: ") + strerror(errno);
        return written;
    }
    self->text.append(data, n); self->bytes += n; return n;
}
void WebTransfer::close() {
    if (multi && easy) curl_multi_remove_handle(multi, easy);
    if (easy) curl_easy_cleanup(easy);
    if (multi) curl_multi_cleanup(multi);
    easy = NULL; multi = NULL;
    if (file) {
        if (fclose(file)) { success = false; failure = "Could not finish saving the download. Check free storage."; }
        file = NULL;
    }
    if (!success && fileOpened && !path.empty()) unlink(path.c_str());
    fileOpened = false;
}
bool WebTransfer::start(const std::string &url, const std::string &output, size_t maximum) {
    close(); path = output; text.clear(); failure.clear(); details[0] = 0;
    bytes = length = 0; limit = maximum; success = false; finished = false;
    if (url.find("https://") != 0) failure = "An insecure update URL was rejected.";
    else if (!path.empty() && !(file = fopen(path.c_str(), "wb")))
        failure = std::string("Could not save the download: ") + strerror(errno);
    else if (!(easy = curl_easy_init()) || !(multi = curl_multi_init()))
        failure = "Could not initialize the network connection.";
    fileOpened = file != NULL;
    if (!failure.empty()) { finished = true; close(); return false; }
    curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, details);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(easy, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(easy, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
    const char *ca = std::getenv("SSL_CERT_FILE");
    if (ca && *ca) curl_easy_setopt(easy, CURLOPT_CAINFO, ca);
    curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT, path.empty() ? 60L : 600L);
    curl_easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT, 128L);
    curl_easy_setopt(easy, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(maximum));
    curl_easy_setopt(easy, CURLOPT_USERAGENT, "CoolReader/PocketBook OTA");
    if (curl_multi_add_handle(multi, easy) != CURLM_OK) {
        finished = true; failure = "Could not start the network connection."; close(); return false;
    }
    return true;
}
bool WebTransfer::poll() {
    if (finished) return true;
    int running = 0;
    CURLMcode code = curl_multi_perform(multi, &running);
    if (code != CURLM_OK) { failure = curl_multi_strerror(code); finished = true; close(); return true; }
    curl_off_t size = 0;
    if (curl_easy_getinfo(easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size) == CURLE_OK && size > 0) length = size;
    int remaining;
    while (CURLMsg *message = curl_multi_info_read(multi, &remaining)) {
        if (message->msg != CURLMSG_DONE) continue;
        success = message->data.result == CURLE_OK;
        if (!success && failure.empty()) {
            long status = 0; curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status);
            failure = status >= 400 ? "The update server returned HTTP " + std::to_string(status) + "." :
                (details[0] ? details : curl_easy_strerror(message->data.result));
        }
        finished = true; close(); break;
    }
    return finished;
}
void WebTransfer::cancel() {
    success = false; finished = true; failure = "Download cancelled."; close();
}
