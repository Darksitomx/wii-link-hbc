#include "http.h"
#include "config.h"
#include "debug.h"
#include "util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <network.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define HEADER_CAPACITY 8192

typedef struct {
    char host[256];
    char path[1024];
    uint16_t port;
} ParsedUrl;

typedef struct {
    int socket;
    uint8_t prefix[HEADER_CAPACITY];
    size_t prefix_pos;
    size_t prefix_size;
} BodyStream;

static char g_error[256];

static int fail(const char *message) {
    snprintf(g_error, sizeof(g_error), "%s", message);
    ERROR("HTTP: %s", g_error);
    return -1;
}

static int fail_code(const char *message, int code) {
    snprintf(g_error, sizeof(g_error), "%s (%d)", message, code);
    ERROR("HTTP: %s", g_error);
    return -1;
}

const char *http_last_error(void) { return g_error; }

static int parse_url(const char *url, ParsedUrl *parsed) {
    const char prefix[] = "http://";
    if (strncmp(url, prefix, sizeof(prefix) - 1) != 0) return fail("Solo se admiten URL http://");
    const char *host_start = url + sizeof(prefix) - 1;
    const char *path = strchr(host_start, '/');
    const char *host_end = path ? path : url + strlen(url);
    const char *colon = NULL;
    for (const char *p = host_start; p < host_end; ++p) if (*p == ':') colon = p;
    size_t host_len = (size_t)((colon ? colon : host_end) - host_start);
    if (!host_len || host_len >= sizeof(parsed->host)) return fail("Host HTTP no valido");
    memcpy(parsed->host, host_start, host_len);
    parsed->host[host_len] = '\0';
    parsed->port = colon ? (uint16_t)atoi(colon + 1) : 80;
    snprintf(parsed->path, sizeof(parsed->path), "%s", path ? path : "/");
    if (strlen(parsed->path) >= sizeof(parsed->path) - 1) return fail("Ruta HTTP demasiado larga");
    return 0;
}

static int send_all(int socket, const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    while (size) {
        int sent = net_send(socket, p, (s32)size, 0);
        if (sent <= 0) return fail_code("Fallo enviando solicitud", sent);
        p += sent;
        size -= (size_t)sent;
    }
    return 0;
}

static int open_socket(const ParsedUrl *url) {
    struct hostent *host = net_gethostbyname(url->host);
    if (!host || !host->h_addr_list || !host->h_addr_list[0]) return fail("No se pudo resolver DNS");
    int socket = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (socket < 0) return fail_code("No se pudo crear socket", socket);

    struct timeval timeout = {
        .tv_sec = HTTP_IO_TIMEOUT_MS / 1000,
        .tv_usec = (HTTP_IO_TIMEOUT_MS % 1000) * 1000
    };
    net_setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    net_setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(url->port);
    memcpy(&address.sin_addr, host->h_addr_list[0], sizeof(address.sin_addr));
    int rc = net_connect(socket, (struct sockaddr *)&address, sizeof(address));
    if (rc < 0) {
        net_close(socket);
        return fail_code("No se pudo conectar al servidor", rc);
    }
    return socket;
}

static char *find_header_end(char *data, size_t size) {
    if (size < 4) return NULL;
    for (size_t i = 0; i + 3 < size; ++i) {
        if (!memcmp(data + i, "\r\n\r\n", 4)) return data + i;
    }
    return NULL;
}

static int stream_byte(BodyStream *stream, uint8_t *out) {
    if (stream->prefix_pos < stream->prefix_size) {
        *out = stream->prefix[stream->prefix_pos++];
        return 1;
    }
    int got = net_recv(stream->socket, out, 1, 0);
    return got;
}

static int stream_read(BodyStream *stream, void *output, size_t wanted) {
    uint8_t *out = (uint8_t *)output;
    size_t done = 0;
    if (stream->prefix_pos < stream->prefix_size) {
        size_t available = stream->prefix_size - stream->prefix_pos;
        size_t take = available < wanted ? available : wanted;
        memcpy(out, stream->prefix + stream->prefix_pos, take);
        stream->prefix_pos += take;
        done += take;
    }
    if (done < wanted) {
        int got = net_recv(stream->socket, out + done, (s32)(wanted - done), 0);
        if (got > 0) done += (size_t)got;
        else if (!done) return got;
    }
    return (int)done;
}

static int read_line(BodyStream *stream, char *line, size_t capacity) {
    size_t used = 0;
    while (used + 1 < capacity) {
        uint8_t c;
        int got = stream_byte(stream, &c);
        if (got <= 0) return -1;
        if (c == '\n') {
            if (used && line[used - 1] == '\r') --used;
            line[used] = '\0';
            return 0;
        }
        line[used++] = (char)c;
    }
    return -1;
}

typedef int (*DataSink)(const uint8_t *data, size_t size, void *user);

static int receive_body(BodyStream *stream, bool chunked, uint64_t length,
                        DataSink sink, void *sink_user, HttpProgress progress, void *progress_user) {
    uint8_t buffer[8192];
    uint64_t done = 0;
    if (progress) progress(0, length, progress_user);

    if (chunked) {
        char line[80];
        for (;;) {
            if (read_line(stream, line, sizeof(line)) != 0) return fail("Respuesta chunked truncada");
            char *extension = strchr(line, ';');
            if (extension) *extension = '\0';
            uint64_t chunk_remaining = strtoull(line, NULL, 16);
            if (!chunk_remaining) {
                /* Consume trailers until the empty line. */
                do { if (read_line(stream, line, sizeof(line)) != 0) break; } while (*line);
                break;
            }
            while (chunk_remaining) {
                size_t want = chunk_remaining > sizeof(buffer) ? sizeof(buffer) : (size_t)chunk_remaining;
                int got = stream_read(stream, buffer, want);
                if (got <= 0) return fail("Cuerpo HTTP chunked incompleto");
                if (sink(buffer, (size_t)got, sink_user) != 0) return -1;
                done += (size_t)got;
                chunk_remaining -= (size_t)got;
                if (progress) progress(done, 0, progress_user);
            }
            uint8_t crlf[2];
            if (stream_read(stream, crlf, 2) != 2 || crlf[0] != '\r' || crlf[1] != '\n')
                return fail("Separador chunked no valido");
        }
    } else if (length) {
        while (done < length) {
            size_t want = length - done > sizeof(buffer) ? sizeof(buffer) : (size_t)(length - done);
            int got = stream_read(stream, buffer, want);
            if (got <= 0) return fail("Descarga HTTP incompleta");
            if (sink(buffer, (size_t)got, sink_user) != 0) return -1;
            done += (size_t)got;
            if (progress) progress(done, length, progress_user);
        }
    } else {
        for (;;) {
            int got = stream_read(stream, buffer, sizeof(buffer));
            if (got == 0) break;
            if (got < 0) return fail_code("Error recibiendo HTTP", got);
            if (sink(buffer, (size_t)got, sink_user) != 0) return -1;
            done += (size_t)got;
            if (progress) progress(done, 0, progress_user);
        }
    }
    return 0;
}

static int request(const char *url, DataSink sink, void *sink_user,
                   HttpProgress progress, void *progress_user) {
    ParsedUrl parsed;
    if (parse_url(url, &parsed) != 0) return -1;
    TRACE("GET %s", url);
    int socket = open_socket(&parsed);
    if (socket < 0) return -1;

    char request_data[1536];
    int request_len = snprintf(request_data, sizeof(request_data),
        "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: WiiLink-Patcher-Wii/%s\r\n"
        "Accept: */*\r\nConnection: close\r\n\r\n",
        parsed.path, parsed.host, APP_VERSION);
    if (request_len <= 0 || (size_t)request_len >= sizeof(request_data) ||
        send_all(socket, request_data, (size_t)request_len) != 0) {
        net_close(socket);
        return -1;
    }

    char raw[HEADER_CAPACITY];
    size_t used = 0;
    char *header_end = NULL;
    while (used < sizeof(raw) - 1 && !header_end) {
        int got = net_recv(socket, raw + used, (s32)(sizeof(raw) - 1 - used), 0);
        if (got <= 0) { net_close(socket); return fail("Cabecera HTTP incompleta"); }
        used += (size_t)got;
        raw[used] = '\0';
        header_end = find_header_end(raw, used);
    }
    if (!header_end) { net_close(socket); return fail("Cabecera HTTP demasiado grande"); }
    size_t header_size = (size_t)(header_end - raw) + 4;

    int status = 0;
    if (sscanf(raw, "HTTP/%*u.%*u %d", &status) != 1) {
        net_close(socket); return fail("Estado HTTP no valido");
    }
    if (status != 200) {
        net_close(socket); return fail_code("Servidor devolvio estado HTTP", status);
    }

    bool chunked = false;
    uint64_t length = 0;
    char *line = strstr(raw, "\r\n") + 2;
    while (line && line < header_end) {
        char *end = strstr(line, "\r\n");
        if (!end || end > header_end) break;
        *end = '\0';
        if (!strncasecmp(line, "Content-Length:", 15)) length = strtoull(line + 15, NULL, 10);
        if (!strncasecmp(line, "Transfer-Encoding:", 18) && strstr(line + 18, "chunked")) chunked = true;
        line = end + 2;
    }

    BodyStream stream = { .socket = socket, .prefix_pos = 0, .prefix_size = used - header_size };
    if (stream.prefix_size) memcpy(stream.prefix, raw + header_size, stream.prefix_size);
    int rc = receive_body(&stream, chunked, length, sink, sink_user, progress, progress_user);
    net_close(socket);
    return rc;
}

static int file_sink(const uint8_t *data, size_t size, void *user) {
    FILE *fp = (FILE *)user;
    if (fwrite(data, 1, size, fp) != size) return fail("Error escribiendo en SD/USB");
    return 0;
}

typedef struct { uint8_t *data; size_t capacity; size_t used; } MemorySink;
static int memory_sink(const uint8_t *data, size_t size, void *user) {
    MemorySink *mem = (MemorySink *)user;
    if (size > mem->capacity - mem->used) return fail("Respuesta HTTP excede el buffer");
    memcpy(mem->data + mem->used, data, size);
    mem->used += size;
    return 0;
}

int http_download(const char *url, const char *destination, HttpProgress progress, void *user) {
    if (ensure_parent_dir(destination) != 0) return fail("No se pudo crear el directorio de destino");
    char partial[520];
    snprintf(partial, sizeof(partial), "%s.part", destination);
    for (int attempt = 1; attempt <= HTTP_RETRIES; ++attempt) {
        unlink(partial);
        FILE *fp = fopen(partial, "wb");
        if (!fp) return fail("No se pudo abrir el archivo de destino");
        int rc = request(url, file_sink, fp, progress, user);
        fflush(fp);
        fclose(fp);
        if (rc == 0) {
            unlink(destination);
            if (rename(partial, destination) != 0) return fail("No se pudo finalizar el archivo descargado");
            INFO("Downloaded %s (%llu bytes)", destination, (unsigned long long)file_size_path(destination));
            return 0;
        }
        WARN("Reintento HTTP %d/%d: %s", attempt, HTTP_RETRIES, g_error);
        sleep(1);
    }
    unlink(partial);
    return -1;
}

int http_get_memory(const char *url, void *buffer, size_t capacity, size_t *out_size) {
    MemorySink sink = { .data = (uint8_t *)buffer, .capacity = capacity, .used = 0 };
    int rc = request(url, memory_sink, &sink, NULL, NULL);
    if (out_size) *out_size = sink.used;
    return rc;
}
