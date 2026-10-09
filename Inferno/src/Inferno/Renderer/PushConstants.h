#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include <cstddef>
#include <cstdint>

// ================================================
// TODO: Maybe Camera stuff could be a serparate push constants struct
// ================================================

namespace Inferno {
struct MeshPushConstants {
  glm::mat4 Mvp;
  glm::mat4 Model;
  uint32_t EntityID;
};

struct OutlinePushConstants {
  glm::vec3 Color;
  uint32_t EntityID;
  uint32_t ThicknessPX;
};

struct GridPushConstants {
  glm::mat4 ViewProjection;
  glm::mat4 InverseViewProjection;
};

static_assert(sizeof(GridPushConstants) == 128);

struct DebugLinePushConstants {
  glm::mat4 View;
  glm::mat4 Proj;
};

struct GBufferDebugPushConstants {
  glm::mat4 InverseProjection;
  float NearPlane;
  float FarPlane;
  uint32_t ViewMode;
};

static_assert(offsetof(GBufferDebugPushConstants, NearPlane) == 64);
static_assert(offsetof(GBufferDebugPushConstants, FarPlane) == 68);
static_assert(offsetof(GBufferDebugPushConstants, ViewMode) == 72);

struct DeferredLightingPushConstants {
  glm::mat4 InverseViewProjection;
  glm::vec4 LightDirection;
  glm::vec4 LightColorIntensity;
  glm::vec4 Settings;
  glm::uvec4 LightCounts;
};

static_assert(sizeof(DeferredLightingPushConstants) == 128);
static_assert(offsetof(DeferredLightingPushConstants, LightDirection) == 64);
static_assert(offsetof(DeferredLightingPushConstants, LightColorIntensity) ==
              80);
static_assert(offsetof(DeferredLightingPushConstants, Settings) == 96);
static_assert(offsetof(DeferredLightingPushConstants, LightCounts) == 112);

struct TiledLightCullingPushConstants {
  glm::mat4 ViewProjection;
  glm::vec4 ProjectionScreen;
  glm::uvec4 LightGrid;
  glm::uvec4 SpotLightGrid;
};

static_assert(sizeof(TiledLightCullingPushConstants) == 112);
static_assert(offsetof(TiledLightCullingPushConstants, ProjectionScreen) ==
              64);
static_assert(offsetof(TiledLightCullingPushConstants, LightGrid) == 80);
static_assert(offsetof(TiledLightCullingPushConstants, SpotLightGrid) == 96);

struct ToneMappingPushConstants {
  float Exposure;
};

static_assert(sizeof(ToneMappingPushConstants) == 4);
} // namespace Inferno
