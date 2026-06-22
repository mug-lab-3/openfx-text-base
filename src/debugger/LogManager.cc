#include "LogManager.h"

#include <glaze/glaze.hpp>

#ifdef ENABLE_UDP_LOG

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <chrono>
#include <functional>
#include <mutex>
#include <string_view>
#include <thread>

namespace MugLab::OfxBase::debugger {

struct LogContext {
    SOCKET sock_ = INVALID_SOCKET;
    sockaddr_in addr_{};
    bool initialized_ = false;
    std::mutex mutex_;

    ~LogContext() {
        if (sock_ != INVALID_SOCKET) {
            closesocket(sock_);
        }
#ifdef _WIN32
        if (initialized_) {
            WSACleanup();
        }
#endif
    }
};

static auto getContext() -> LogContext& {
    static LogContext ctx;
    if (!ctx.initialized_) {
        std::lock_guard<std::mutex> lock(ctx.mutex_);
        if (!ctx.initialized_) {
#ifdef _WIN32
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                return ctx;
            }
#endif
            ctx.sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (ctx.sock_ != INVALID_SOCKET) {
                ctx.addr_.sin_family = AF_INET;
                ctx.addr_.sin_port = htons(LogManager::kDefaultPort);
                ctx.addr_.sin_addr.s_addr = inet_addr(LogManager::kDefaultHost);
                ctx.initialized_ = true;
            }
        }
    }
    return ctx;
}

struct DynamicLogEntry {
    std::string_view app_ = "OfxBase";
    uint64_t timestamp_{};
    uint64_t tid_{};
    std::string_view level_;
    std::string_view tag_;
    std::string_view type_;
    glz::raw_json payload_;
};

}  // namespace MugLab::OfxBase::debugger

template <>
struct glz::meta<MugLab::OfxBase::debugger::DynamicLogEntry> {
    using T = MugLab::OfxBase::debugger::DynamicLogEntry;
    static constexpr auto value = object("app", &T::app_, "timestamp", &T::timestamp_, "tid", &T::tid_, "level",
                                         &T::level_, "tag", &T::tag_, "type", &T::type_, "payload", &T::payload_);
};

namespace MugLab::OfxBase::debugger {

void LogManager::logInternal(std::string_view level, std::string_view tag, std::string_view type, const LogValue* args,
                             size_t count) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());

    std::string payloadJson;
    if (count == 1) {
        const auto& val = args[0];
        switch (val.type_) {
            case LogManager::LogValue::Type::String:
                // Manually quote and escape string if needed, or just wrap it
                payloadJson = "\"" + std::string(val.val_.stringVal_) + "\"";
                break;
            case LogManager::LogValue::Type::Int:
                payloadJson = std::to_string(val.val_.intVal_);
                break;
            case LogManager::LogValue::Type::Uint:
                payloadJson = std::to_string(val.val_.uintVal_);
                break;
            case LogManager::LogValue::Type::Double:
                payloadJson = std::to_string(val.val_.doubleVal_);
                break;
            case LogManager::LogValue::Type::Bool:
                payloadJson = val.val_.boolVal_ ? "true" : "false";
                break;
        }
    } else {
        payloadJson = "[";
        for (size_t i = 0; i < count; ++i) {
            if (i > 0) {
                payloadJson += ",";
            }
            std::string v;
            const auto& val = args[i];
            switch (val.type_) {
                case LogManager::LogValue::Type::String:
                    glz::write_json(val.val_.stringVal_, v);
                    break;
                case LogManager::LogValue::Type::Int:
                    v = std::to_string(val.val_.intVal_);
                    break;
                case LogManager::LogValue::Type::Uint:
                    v = std::to_string(val.val_.uintVal_);
                    break;
                case LogManager::LogValue::Type::Double:
                    v = std::to_string(val.val_.doubleVal_);
                    break;
                case LogManager::LogValue::Type::Bool:
                    v = val.val_.boolVal_ ? "true" : "false";
                    break;
            }
            payloadJson += v;
        }
        payloadJson += "]";
    }

    DynamicLogEntry entry{.timestamp_ = static_cast<uint64_t>(ms),
                          .tid_ = static_cast<uint64_t>(tid),
                          .level_ = level,
                          .tag_ = tag,
                          .type_ = type,
                          .payload_ = {payloadJson}};

    std::string json;
    glz::write_json(entry, json);
    sendRaw(json + "\n");
}

void LogManager::sendRaw(const std::string& message) {
    auto& ctx = getContext();
    if (ctx.initialized_ && ctx.sock_ != INVALID_SOCKET) {
        sendto(ctx.sock_, message.c_str(), static_cast<int>(message.length()), 0,
               reinterpret_cast<const struct sockaddr*>(&ctx.addr_), sizeof(ctx.addr_));
    }
}

}  // namespace MugLab::OfxBase::debugger

#else

namespace MugLab::OfxBase::debugger {
void LogManager::logInternal(std::string_view /*unused*/, std::string_view /*unused*/, std::string_view /*unused*/,
                             const LogValue* /*unused*/, size_t /*unused*/) {
}
void LogManager::sendRaw(const std::string& /*unused*/) {
}
}  // namespace MugLab::OfxBase::debugger

#endif
