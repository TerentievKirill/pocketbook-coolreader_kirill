#include "crengine.h"
#include "../cr3gui/src/web.h"
#include "../cr3gui/src/web_transfer.h"
#include <unistd.h>
#include "testutil.h"
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
lString32 pbSkinFileName, openedCacheFile, currentCacheDir;
class Callbacks : public web {
public:
    using web::writer;
    using web::writerBinary;
};
int main(int argc, char **argv) {
    CHECK(argc == 5);
    setenv("NO_PROXY", "127.0.0.1,localhost", 1);
    CHECK(!curl_global_init(CURL_GLOBAL_DEFAULT));
    std::string base = argv[1], path = std::string(argv[4]) + "/download.bin";
    setenv("SSL_CERT_FILE", argv[2], 1);
    CHECK(web::get(base + "/ok") == "TLS verified\n");
    CHECK(web::getBinary(base + "/ok", path));
    std::ifstream file(path, std::ios::binary);
    std::string downloaded((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    CHECK(downloaded == "TLS verified\n");
    CHECK(!web::getBinary(base + "/ok", path + "/missing/child"));
    CHECK(web::get(base + "/redirect-file").empty());
    setenv("SSL_CERT_FILE", argv[3], 1);
    CHECK(web::get(base + "/ok").empty());
    CHECK(!web::getBinary(base + "/ok", path));
    setenv("SSL_CERT_FILE", argv[2], 1);
    std::string wrongHost = base;
    wrongHost.replace(wrongHost.find("127.0.0.1"), 9, "localhost");
    CHECK(web::get(wrongHost + "/ok").empty());
    std::string buffer;
    char value[] = "abcdef";
    CHECK(Callbacks::writer(value, 2, 3, &buffer) == 6 && buffer == "abcdef");
    CHECK(Callbacks::writer(value, std::numeric_limits<size_t>::max(), 2, &buffer) == 0);
    FILE *temporary = tmpfile(); CHECK(temporary);
    CHECK(Callbacks::writerBinary(value, 2, 3, temporary) == 6);
    CHECK(!fclose(temporary));
    // Timer-driven transfers: progress, cancellation, HTTP/TLS and local errors.
    WebTransfer transfer;
    CHECK(transfer.start(base + "/ok", "", 4096));
    while (!transfer.poll()) usleep(1000);
    CHECK(transfer.succeeded() && transfer.body() == "TLS verified\n");
    CHECK(transfer.start(base + "/slow", path, 256*1024));
    bool intermediate = false;
    while (!transfer.poll()) {
        if (transfer.received() > 0 && transfer.total() > transfer.received()) intermediate = true;
        usleep(1000);
    }
    CHECK(transfer.succeeded() && intermediate && transfer.received() == 128*1024);
    CHECK(transfer.start(base + "/slow", path, 256*1024));
    while (!transfer.poll() && transfer.received() == 0) usleep(1000);
    CHECK(transfer.received() > 0); transfer.cancel();
    CHECK(transfer.poll() && !transfer.succeeded() && access(path.c_str(),F_OK) != 0);
    CHECK(transfer.start(base + "/missing", "", 4096));
    while (!transfer.poll()) usleep(1000);
    CHECK(!transfer.succeeded() && transfer.error().find("404") != std::string::npos);
    CHECK(transfer.start(base + "/ok", "", 4));
    while (!transfer.poll()) usleep(1000);
    CHECK(!transfer.succeeded());
    CHECK(!transfer.start(base + "/ok", path + "/missing/child", 4096));
    CHECK(!transfer.error().empty());
    setenv("SSL_CERT_FILE",argv[3],1);
    CHECK(transfer.start(base + "/ok", "", 4096));
    while (!transfer.poll()) usleep(1000);
    CHECK(!transfer.succeeded() && !transfer.error().empty());
    setenv("SSL_CERT_FILE",argv[2],1);
    CHECK(transfer.start(wrongHost + "/ok", "", 4096));
    while (!transfer.poll()) usleep(1000);
    CHECK(!transfer.succeeded());
    CHECK(transfer.start(base + "/redirect-file", "", 4096));
    while (!transfer.poll()) usleep(1000);
    CHECK(!transfer.succeeded());
    CHECK(!transfer.start("http://example.invalid/", "", 4096));
    curl_global_cleanup();
    puts("PASS: trusted TLS text/binary downloads, rejected untrusted certificates and hostnames, blocked file redirects, callback byte counts; asynchronous progress/cancel/HTTP/TLS/storage errors");
    return 0;
}
