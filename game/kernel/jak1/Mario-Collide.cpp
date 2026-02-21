#include "mario1.h"

void MarioManager::UpdateActorCollisions() {
  if (!m_run_collide) return;  // Step 1: Pause if not running

  // Assume primary Mario ID is the first active one for distance checks
  auto active_ids = GetActiveMarioIds();
  if (active_ids.empty()) return;
  int main_mario_id = active_ids[0];
  MarioInstance* mario = GetMario(main_mario_id);
  if (!mario) return;

  float mario_pos[3] = {mario->state.position[0], mario->state.position[1], mario->state.position[2]};

  constexpr float DIST_THRESHOLD = 10000.f;  // Adjustable threshold
  constexpr float DIST_THRESHOLD_SQ = DIST_THRESHOLD * DIST_THRESHOLD;
 // printf("[Mario Collide] Checking for actors to clean...\n");
  // Step 2: Cull far actors
  for (auto it = m_actor_infos.begin(); it != m_actor_infos.end(); ++it) {
    auto& info = it->second;
    if (info.is_spawned) {
      float dx = info.pos[0] - mario_pos[0];
      float dy = info.pos[1] - mario_pos[1];
      float dz = info.pos[2] - mario_pos[2];
      float dist_sq = dx * dx + dy * dy + dz * dz;
      if (dist_sq > DIST_THRESHOLD_SQ) {

        sm64_surface_object_delete(info.sm64_id);
        delete[] info.mesh;
        info.mesh = nullptr;
        info.is_spawned = false;
        printf("[Mario Collide] Killed far actor %s\n", info.name.c_str());
      }
    }
  }
}

void MarioManager::AddTestActors()
{
    // Clear existing test data first (optional - comment out if you want to keep real data)
    // m_actor_infos.clear();

    printf("[Mario Collide] Adding test actors for debugging...\n");

    // ─────────────────────────────────────────────────────────────
    // Test Actor 1: A simple static crate near origin
    // ─────────────────────────────────────────────────────────────
    {
        ActorInfo test;
        test.name         = "test_crate_01";
        test.pos[0]       = 0.0f;     // already in Mario units (meters)
        test.pos[1]       = 2.0f;
        test.pos[2]       = 5.0f;
        test.euler_rot[0] = 0.0f;
        test.euler_rot[1] = 45.0f;    // 45° yaw
        test.euler_rot[2] = 0.0f;
        test.is_platform  = false;
        test.is_spawned   = false;    // we'll pretend it needs spawning
        test.num_surfaces = 12;       // fake triangle count
        test.mesh_hash    = 0xDEADBEEF;
        test.sm64_id      = 0;

        // Optional: fake a tiny mesh (just for testing hash/change detection)
        // You can leave mesh = nullptr for pure position testing
        test.mesh = nullptr;

        m_actor_infos[test.name] = test;
        printf("[Test] Added %s at (%.1f, %.1f, %.1f)\n", 
               test.name.c_str(), test.pos[0], test.pos[1], test.pos[2]);
    }

    // ─────────────────────────────────────────────────────────────
    // Test Actor 2: A moving platform far away (should get culled)
    // ─────────────────────────────────────────────────────────────
    {
        ActorInfo test;
        test.name         = "test_moving_plat_99";
        test.pos[0]       = 15000.0f;   // way outside 10k threshold
        test.pos[1]       = 0.0f;
        test.pos[2]       = 20000.0f;
        test.euler_rot[0] = 0.0f;
        test.euler_rot[1] = 0.0f;
        test.euler_rot[2] = 0.0f;
        test.is_platform  = true;
        test.is_spawned   = true;       // pretend it's already active
        test.num_surfaces = 2;
        test.mesh_hash    = 0x12345678;
        test.sm64_id      = 999;        // fake libsm64 ID

        m_actor_infos[test.name] = test;
        printf("[Test] Added far-away %s at (%.1f, %.1f, %.1f) → should be culled\n", 
               test.name.c_str(), test.pos[0], test.pos[1], test.pos[2]);
    }

    // ─────────────────────────────────────────────────────────────
    // Test Actor 3: Something close that should stay
    // ─────────────────────────────────────────────────────────────
    {
        ActorInfo test;
        test.name         = "test_barrel_near";
        test.pos[0]       = 3.0f;
        test.pos[1]       = 1.5f;
        test.pos[2]       = -4.0f;
        test.euler_rot[0] = 10.0f;
        test.euler_rot[1] = -20.0f;
        test.euler_rot[2] = 5.0f;
        test.is_platform  = false;
        test.is_spawned   = false;
        test.num_surfaces = 8;
        test.mesh_hash    = 0xCAFEBABE;

        m_actor_infos[test.name] = test;
        printf("[Test] Added close %s at (%.1f, %.1f, %.1f)\n", 
               test.name.c_str(), test.pos[0], test.pos[1], test.pos[2]);
    }

    printf("[Mario Collide] Test actors added. Total in map = %zu\n", m_actor_infos.size());
}
