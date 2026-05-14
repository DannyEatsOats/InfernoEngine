#pragma once

// #include "Core.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Inferno {
class Log {
public:
  static void Init();
  static std::shared_ptr<spdlog::logger> &GetInfernoLogger() {
    return s_InfernoLogger;
  }
  static std::shared_ptr<spdlog::logger> &GetGameLogger() {
    return s_GameLogger;
  }

private:
  static std::shared_ptr<spdlog::logger> s_InfernoLogger;
  static std::shared_ptr<spdlog::logger> s_GameLogger;
};
} // namespace Inferno

#define INFERNO_LOG_FATAL(...)                                                 \
  ::Inferno::Log::GetInfernoLogger()->fatal(__VA_ARGS__)
#define INFERNO_LOG_ERROR(...)                                                 \
  ::Inferno::Log::GetInfernoLogger()->error(__VA_ARGS__)
#define INFERNO_LOG_WARN(...)                                                  \
  ::Inferno::Log::GetInfernoLogger()->warn(__VA_ARGS__)
#define INFERNO_LOG_INFO(...)                                                  \
  ::Inferno::Log::GetInfernoLogger()->info(__VA_ARGS__)
#define INFERNO_LOG_TRACE(...)                                                 \
  ::Inferno::Log::GetInfernoLogger()->trace(__VA_ARGS__)

// Client Log macro
#define GAME_LOG_FATAL(...) ::Inferno::Log::GetGameLogger()->fatal(__VA_ARGS__)
#define GAME_LOG_ERROR(...) ::Inferno::Log::GetGameLogger()->error(__VA_ARGS__)
#define GAME_LOG_WARN(...) ::Inferno::Log::GetGameLogger()->warn(__VA_ARGS__)
#define GAME_LOG_INFO(...) ::Inferno::Log::GetGameLogger()->info(__VA_ARGS__)
#define GAME_LOG_TRACE(...) ::Inferno::Log::GetGameLogger()->trace(__VA_ARGS__)
