// Refactored Mario1.h
#pragma once
#include "libsm64.h"
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "common/common_types.h"
#include "collide/level.h"

// Constants
constexpr float METERS_TO_UNITS = 50.0f / 4096.0f;

// Individual Mario instance state
struct MarioInstance {
  int id = -1;
  SM64MarioState state = {};
  SM64MarioGeometryBuffers geom = {};
  SM64MarioInputs inputs = {
      .camLookX = 0.0f,
      .camLookZ = 1.0f,
      .stickX = 0.0f,
      .stickY = 0.0f,
      .buttonA = 0,
      .buttonB = 0,
      .buttonZ = 0
  };
  // Texture is shared globally, so not stored here
  bool active = false;
};

//Structures
struct PlatformInfo {
  u32 x_pos;
  u32 y_pos;
  u32 z_pos;
  u32 rot_x;
  u32 rot_y;
  u32 rot_z;
  u32 rot_w;
};

// MarioManager class to encapsulate multiple Mario instances and shared logic
class MarioManager {
 private:
  static MarioManager* sInstance;  // Singleton for the manager itself (shared state)
  static uint8_t* s_shared_texture;  // Shared ROM texture across all Marios

  std::unordered_map<int, std::unique_ptr<MarioInstance>> m_marios;  // Keyed by ID
  std::unordered_map<std::string, uint32_t> m_actor_surface_objects;

  // Shared state (from original globals, can add more as needed)
  PlatformInfo m_platform_info = {0};
  bool m_platform_info_valid = false;

  // Private constructor for singleton
  MarioManager();
  ~MarioManager();

 public:
  // Singleton access
  static MarioManager& Get();

  // Initialization (global setup, formerly load_and_init_mario)
  static void Initialize();

  // Create a new Mario instance
  int CreateMario(float x, float y, float z);

  // Destroy a Mario instance by ID
  void DestroyMario(int id);

  // Main tick/update for all active Marios (formerly tick_mario_frame)
  static void Tick();

  // Update for a specific Mario
  void TickMario(int id);

  // Getters for state (used in pc_ functions) - require ID
  MarioInstance* GetMario(int id);
  const MarioInstance* GetMario(int id) const;
  float GetMarioX(int id) const;
  float GetMarioY(int id) const;
  float GetMarioZ(int id) const;
  int GetMarioId(int id) const { return id; }  // Trivial
  uint32_t GetMarioAction(int id) const;
  SM64MarioGeometryBuffers& GetMarioGeom(int id);
  const SM64MarioGeometryBuffers& GetMarioGeom(int id) const;

  SM64MarioInputs& GetMarioInputs(int id);
  const SM64MarioInputs& GetMarioInputs(int id) const;

  uint8_t* GetSharedTexture() const { return s_shared_texture; }

  // Setters for inputs and state (used in pc_ functions) - require ID
  void SetMarioCamera(int id, float x, float z);
  void SetMarioMusic(int id, uint32_t music_bits);
  void SetMarioPosition(int id, float x, float y, float z);
  void SetMarioWaterLevel(int id, float level);
  void ChangeMarioState(int id, uint32_t act);
  void HealMario(int id);
  void DamageMario(int id);

  // Other methods (refactored from original functions) - shared unless specified
  void UpdateCollide(int id);  // Per-Mario if needed, but collision is in tick
  void UpdatePlatformInfo(u32 platform_info_ptr);
  void UpdateMovingPlatform();
  void UpdatePseudoFloor(int id);
  void MaybeReloadSurfaces(int id);  // Per-Mario position-based


  // Global methods
  void UpdateCollideGlobal();  // For shared dynamic objects
  void MaybeReloadSurfacesGlobal();

  // Shutdown/cleanup
  static void Shutdown();

  // Query active Marios
  size_t GetActiveMarioCount() const { return m_marios.size(); }
  std::vector<int> GetActiveMarioIds() const;
};

  void pc_set_mario_camera(uint32_t x, uint32_t z);
  void pc_set_mario_position_from_goal(int id, uint32_t x_bits, uint32_t y_bits, uint32_t z_bits);
  void pc_set_mario_water_level_from_goal(int id, uint32_t level_bits);
  uint64_t pc_get_mario_x(int id);
  uint64_t pc_get_mario_y(int id);
  uint64_t pc_get_mario_z(int id);


