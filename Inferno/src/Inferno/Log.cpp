#include "Log.h"

#include <memory>

namespace Inferno {
    std::shared_ptr<spdlog::logger> Log::s_InfernoLogger;
    std::shared_ptr<spdlog::logger> Log::s_GameLogger;

    void Log::Init() {
    spdlog::set_pattern("%^[%T] %n: %v%$");
    s_InfernoLogger = spdlog::stdout_color_mt("Inferno");
    s_InfernoLogger->set_level(spdlog::level::trace);
  
    s_GameLogger = spdlog::stdout_color_mt("Game");
    s_GameLogger->set_level(spdlog::level::trace);
}
}
