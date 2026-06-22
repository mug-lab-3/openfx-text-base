#pragma once

#include <array>
#include <cstdint>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#ifdef ENABLE_UDP_LOG
#define LOG_INFO(type, ...) \
    MugLab::OfxBase::debugger::LogManager::logStructured("INFO", type, std::source_location::current(), ##__VA_ARGS__)
#define LOG_ERROR(type, ...) \
    MugLab::OfxBase::debugger::LogManager::logStructured("ERROR", type, std::source_location::current(), ##__VA_ARGS__)
#else
#define LOG_INFO(type, ...) ((void)0)
#define LOG_ERROR(type, ...) ((void)0)
#endif

namespace MugLab::OfxBase::debugger {

namespace detail {
constexpr auto extractTag(std::string_view func) -> std::string_view {
    size_t paren = func.find('(');
    if (paren != std::string_view::npos) {
        func = func.substr(0, paren);
    }

    size_t lastColons = func.rfind("::");
    if (lastColons == std::string_view::npos) {
        size_t space = func.rfind(' ');
        return space == std::string_view::npos ? func : func.substr(space + 1);
    }

    std::string_view classAndFunc = func.substr(0, lastColons);
    size_t prevColons = classAndFunc.rfind("::");
    if (prevColons == std::string_view::npos) {
        size_t space = classAndFunc.rfind(' ');
        return space == std::string_view::npos ? classAndFunc : classAndFunc.substr(space + 1);
    }

    return classAndFunc.substr(prevColons + 2);
}
}  // namespace detail

class LogManager {
   public:
    static constexpr uint16_t kDefaultPort = 5555;
    static constexpr const char* kDefaultHost = "127.0.0.1";

    struct LogValue {
        enum class Type : uint8_t {
            String,
            Int,
            Uint,
            Double,
            Bool
        };
        Type type_;
        union V {
            std::string_view stringVal_;
            int64_t intVal_;
            uint64_t uintVal_;
            double doubleVal_;
            bool boolVal_;
            V() {
            }
        } val_;

        explicit LogValue(std::string_view s) : type_(Type::String) {
            val_.stringVal_ = s;
        }
        explicit LogValue(const char* s) : type_(Type::String) {
            val_.stringVal_ = std::string_view(s);
        }
        explicit LogValue(const std::string& s) : type_(Type::String) {
            val_.stringVal_ = std::string_view(s);
        }
        explicit LogValue(bool b) : type_(Type::Bool) {
            val_.boolVal_ = b;
        }

        template <typename T>
        explicit LogValue(T i)
            requires(std::is_integral_v<T> && !std::is_same_v<T, bool>)
        {
            if constexpr (std::is_signed_v<T>) {
                type_ = Type::Int;
                val_.intVal_ = static_cast<int64_t>(i);
            } else {
                type_ = Type::Uint;
                val_.uintVal_ = static_cast<uint64_t>(i);
            }
        }

        template <typename T>
        explicit LogValue(T f)
            requires(std::is_floating_point_v<T>)
            : type_(Type::Double) {
            val_.doubleVal_ = static_cast<double>(f);
        }

        template <typename T>
        explicit LogValue(T e)
            requires(std::is_enum_v<T>)
            : type_(Type::Int) {
            val_.intVal_ = static_cast<int64_t>(e);
        }
    };

    static void logInternal(std::string_view level, std::string_view tag, std::string_view type, const LogValue* args,
                            size_t count);

    template <typename... Args>
    static void logStructured(std::string_view level, std::string_view type, std::source_location loc, Args&&... args) {
#ifdef ENABLE_UDP_LOG
        if constexpr (sizeof...(args) > 0) {
            std::array<LogValue, sizeof...(args)> packedArgs = {LogValue(std::forward<Args>(args))...};
            logInternal(level, detail::extractTag(loc.function_name()), type, packedArgs.data(), packedArgs.size());
        } else {
            logInternal(level, detail::extractTag(loc.function_name()), type, nullptr, 0);
        }
#endif
    }

   private:
    static void sendRaw(const std::string& message);

    LogManager() = default;
};

}  // namespace MugLab::OfxBase::debugger
