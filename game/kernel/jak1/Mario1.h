// Refactored Mario1.h
#pragma once
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "audio.h"

#include "SDL3/SDL.h"
#include "collide/level.h"
#include "decomp/include/seq_ids.h"
#include "decomp/include/sm64_hacked.h"
#include "game/kernel/common/kscheme.h"
#include "libsm64.h"


// Forward declarations
struct PlatformInfo;
extern std::vector<SM64SurfaceObject>
    g_active_debug_objects;  // DECLARATION (kept global for now, can encapsulate later)

struct PlatformInfo {
  u32 x_pos;
  u32 y_pos;
  u32 z_pos;
  u32 rot_x;
  u32 rot_y;
  u32 rot_z;
  u32 rot_w;
};

constexpr float M_PI = 3.14159265358979323846f;

// MarioManager class to encapsulate Mario state and logic
class MarioManager {
 private:
  static MarioManager* sInstance;  // Singleton instance

  uint8_t* m_texture = nullptr;
  int m_id = -1;
  SM64MarioState m_state = {};
  SM64MarioGeometryBuffers m_geom = {};
  SM64MarioInputs m_inputs = {.camLookX = 0.0f,
                              .camLookZ = 1.0f,
                              .stickX = 0.0f,
                              .stickY = 0.0f,
                              .buttonA = 0,
                              .buttonB = 0,
                              .buttonZ = 0};

  // Additional state (from original globals, can add more as needed)
  PlatformInfo m_platform_info = {0};
  bool m_platform_info_valid = false;
  std::unordered_map<std::string, uint32_t> m_actor_surface_objects;

  // Private constructor for singleton
  MarioManager();
  ~MarioManager();

 public:
  // Singleton access
  static MarioManager& Get();

  // Initialization (formerly load_and_init_mario)
  static void Initialize();

  // Main tick/update (formerly tick_mario_frame)
  static void Tick();

  // Getters for state (used in pc_ functions)
  float GetX() const { return m_state.position[0]; }
  float GetY() const { return m_state.position[1]; }
  float GetZ() const { return m_state.position[2]; }
  int GetId() const { return m_id; }
  uint32_t GetAction() const { return m_state.action; }
  SM64MarioGeometryBuffers& GetGeom() { return m_geom; }
  const SM64MarioGeometryBuffers& GetGeom() const { return m_geom; }

  SM64MarioInputs& GetInputs() { return m_inputs; }
  const SM64MarioInputs& GetInputs() const { return m_inputs; }

  uint8_t* GetTexture() const { return m_texture; }

  // Setters for inputs and state (used in pc_ functions)
  void SetCamera(float x, float z);
  void SetMusic(uint32_t music_bits);
  void SetPosition(float x, float y, float z);
  void SetWaterLevel(float level);
  void ChangeState(uint32_t act);
  void Heal();
  void Damage();

  // Other methods (refactored from original functions)
  void UpdateCollide();
  void UpdatePlatformInfo(u32 platform_info_ptr);
  void UpdateMovingPlatform();
  void UpdatePseudoFloor();
  void MaybeReloadSurfaces();

  // Shutdown/cleanup
  static void Shutdown();
};

// Constants
constexpr float METERS_TO_UNITS = 50.0f / 4096.0f;

// Exposed functions (pc_ wrappers that delegate to MarioManager)
uint64_t pc_get_mario_x();
uint64_t pc_get_mario_y();
uint64_t pc_get_mario_z();
uint64_t pc_get_mario_action();

void pc_set_mario_camera(uint32_t x, uint32_t z);
void pc_set_mario_music_from_goal(uint32_t music_bits);
void pc_set_mario_position_from_goal(uint32_t x_bits, uint32_t y_bits, uint32_t z_bits);
void pc_set_mario_water_level_from_goal(uint32_t level_bits);
void pc_change_mario_state(uint32_t act_bits);
void pc_add_tris_to_surface(uint32_t x_bits,
                            uint32_t y_bits,
                            uint32_t z_bits,
                            uint32_t vert_index_bits,
                            uint32_t name_ptr);
void update_platform_info_from_goal(u32 platform_info_ptr);  // Delegates to manager
void update_moving_platform();                               // Delegates to manager
void pc_spawn_mario_test_collide(uint32_t name_ptr);
void update_psuedo_floor_under_mario();  // Delegates to manager
void pc_mario_says_so_long_gay_bowsa(uint32_t music_bits);
void pc_heal_mario();
void pc_damage_mario();

void load_combined_static_surfaces(const SM64Surface* surfaces1,
                                   int count1,
                                   const SM64Surface* surfaces2,
                                   int count2);
int load_surfaces_near(float x, float y, float z);
void pc_call_load_combined_static_surfaces_from_game_idx(uint32_t x_bits, uint32_t z_bits);

bool point_in_triangle_2d(float px, float pz, const int32_t v[3][3]);
bool triangle_samples_in_cylinder(float center_x, float center_z, const int32_t v[3][3]);
bool run_and_render_mario();
bool has_blue_eco();
bool should_render_mario();
