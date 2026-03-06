#include "mario1.h"

void MarioManager::CleanupDistantActorCollide() {
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

void MarioManager::AddOrUpdateTrisToTempBuffer(
    const char* name,           // ignored in test mode
    uint32_t x_bits,            // ignored
    uint32_t y_bits,
    uint32_t z_bits,
    uint32_t count_bits)
{
    // ─────────────────────────────────────────────────────────────
    // Hardcoded TEST MODE - ignore all inputs
    // ─────────────────────────────────────────────────────────────
    printf("\n[TRIS-TEST] =============================================\n");
    printf("[TRIS-TEST] Hardcoded test mode activated!\n");
    // printf("[TRIS-TEST] Ignoring inputs → name='%s', bits=0x%08X 0x%08X 0x%08X 0x%08X\n",
    //        name ? name : "(null)", x_bits, y_bits, z_bits, count_bits);

    const char* test_name = "test-cube";  // fixed actor name for testing

    // Make sure the test actor exists (create it if missing)
    auto it = m_actor_infos.find(test_name);
    if (it == m_actor_infos.end()) {
        printf("[TRIS-TEST] Creating test actor '%s'\n", test_name);
        
        ActorInfo new_info{};
        new_info.name         = test_name;
        new_info.pos[0]       = 0.0f;
        new_info.pos[1]       = 0.0f;
        new_info.pos[2]       = 0.0f;
        new_info.euler_rot[0] = 0.0f;
        new_info.euler_rot[1] = 0.0f;
        new_info.euler_rot[2] = 0.0f;
        new_info.is_platform  = false;
        new_info.is_spawned   = false;
        new_info.sm64_id      = 0;
        new_info.mesh         = nullptr;
        new_info.num_surfaces = 0;
        new_info.mesh_hash    = 0;

        auto [ins_it, inserted] = m_actor_infos.emplace(test_name, std::move(new_info));
        if (!inserted) {
            printf("[TRIS-TEST] Failed to create test actor '%s'\n", test_name);
            return;
        }
        it = ins_it;
    }

    ActorInfo& info = it->second;

    // Clear previous test data (optional - comment out if you want to accumulate)
    info.vertex_accum.clear();
    info.temp_tris.clear();
    printf("[TRIS-TEST] Cleared previous accum (%zu) and tris (%zu)\n",
           info.vertex_accum.size(), info.temp_tris.size());

    // ─────────────────────────────────────────────────────────────
    // Add 6 hardcoded triangles (24 vertex pushes)
    // Each triangle: 3 vertices + 1 commit call
    // ─────────────────────────────────────────────────────────────
    for (int tri = 1; tri <= 6; ++tri) {
        printf("[TRIS-TEST] Adding triangle #%d / 6\n", tri);

        // Hardcoded vertices - simple cube-like shape centered at origin
        float verts[3][3] = {
            { tri*1.0f,  tri*1.0f,  tri*1.0f },   // v0
            { tri*1.0f, -tri*1.0f,  tri*1.0f },   // v1
            {-tri*1.0f, -tri*1.0f, -tri*1.0f }    // v2
        };

        // Vertex 1
        info.vertex_accum.push_back(verts[0][0]);
        info.vertex_accum.push_back(verts[0][1]);
        info.vertex_accum.push_back(verts[0][2]);
        printf("[TRIS-TEST]   v1: (%.1f, %.1f, %.1f)   accum=%zu\n",
               verts[0][0], verts[0][1], verts[0][2], info.vertex_accum.size());

        // Vertex 2
        info.vertex_accum.push_back(verts[1][0]);
        info.vertex_accum.push_back(verts[1][1]);
        info.vertex_accum.push_back(verts[1][2]);
        printf("[TRIS-TEST]   v2: (%.1f, %.1f, %.1f)   accum=%zu\n",
               verts[1][0], verts[1][1], verts[1][2], info.vertex_accum.size());

        // Vertex 3
        info.vertex_accum.push_back(verts[2][0]);
        info.vertex_accum.push_back(verts[2][1]);
        info.vertex_accum.push_back(verts[2][2]);
        printf("[TRIS-TEST]   v3: (%.1f, %.1f, %.1f)   accum=%zu\n",
               verts[2][0], verts[2][1], verts[2][2], info.vertex_accum.size());

        // Commit (simulate count=4)
        size_t n = info.vertex_accum.size();
        if (n >= 9) {
            SM64Surface tri{};
            tri.type    = 0;
            tri.force   = 0;
            tri.terrain = 0;

            size_t base = n - 9;
            tri.vertices[0][0] = static_cast<int32_t>(std::round(info.vertex_accum[base + 0]));
            tri.vertices[0][1] = static_cast<int32_t>(std::round(info.vertex_accum[base + 1]));
            tri.vertices[0][2] = static_cast<int32_t>(std::round(info.vertex_accum[base + 2]));
            tri.vertices[1][0] = static_cast<int32_t>(std::round(info.vertex_accum[base + 3]));
            tri.vertices[1][1] = static_cast<int32_t>(std::round(info.vertex_accum[base + 4]));
            tri.vertices[1][2] = static_cast<int32_t>(std::round(info.vertex_accum[base + 5]));
            tri.vertices[2][0] = static_cast<int32_t>(std::round(info.vertex_accum[base + 6]));
            tri.vertices[2][1] = static_cast<int32_t>(std::round(info.vertex_accum[base + 7]));
            tri.vertices[2][2] = static_cast<int32_t>(std::round(info.vertex_accum[base + 8]));

            info.temp_tris.push_back(tri);

            printf("[TRIS-TEST] Committed triangle #%zu   v0=(%d,%d,%d)\n",
                   info.temp_tris.size(),
                   tri.vertices[0][0], tri.vertices[0][1], tri.vertices[0][2]);

            // Clean up
            info.vertex_accum.erase(info.vertex_accum.begin(), info.vertex_accum.begin() + base);
        }
    }

    printf("[TRIS-TEST] Done! Added 6 triangles. Total tris now: %zu\n", info.temp_tris.size());
    printf("[TRIS-TEST] =============================================\n\n");
}
void MarioManager::AddOrUpdateActor(const char* name, float x, float y, float z)
{
    if (!name || name[0] == '\0')
    {
        printf("[actor] ERROR: empty name passed - ignored\n");
        return;
    }

    std::string key = name;

    auto it = m_actor_infos.find(key);
    ActorInfo* info = nullptr;

    if (it != m_actor_infos.end())
    {
        info = &it->second;

        if (std::fabs(info->pos[0] - x) < EPS_POSITION &&
            std::fabs(info->pos[1] - y) < EPS_POSITION &&
            std::fabs(info->pos[2] - z) < EPS_POSITION)
        {
            return;
        }

        printf("[actor] Updating '%s' → (%.2f, %.2f, %.2f)\n", name, x, y, z);
    }
    else
    {
        ActorInfo new_info{};
        new_info.name         = key;
        new_info.pos[0]       = x;
        new_info.pos[1]       = y;
        new_info.pos[2]       = z;
        new_info.euler_rot[0] = 0.f;
        new_info.euler_rot[1] = 0.f;
        new_info.euler_rot[2] = 0.f;
        new_info.is_platform  = false;
        new_info.is_spawned   = false;
        new_info.sm64_id      = 0;
        new_info.mesh         = nullptr;
        new_info.num_surfaces = 0;
        new_info.mesh_hash    = 0;

        auto [ins_it, inserted] = m_actor_infos.emplace(key, std::move(new_info));
        if (!inserted) {
            printf("[actor] emplace failed for '%s'\n", name);
            return;
        }

        info = &ins_it->second;
        printf("[actor] New actor '%s' at (%.2f, %.2f, %.2f)\n", name, x, y, z);
    }

    // Update position
    info->pos[0] = x;
    info->pos[1] = y;
    info->pos[2] = z;

    // When actor is created/updated → reset temporary mesh building state
    info->temp_tris.clear();
    info->vertex_accum.clear();

    // If you want to spawn immediately when mesh is ready, do it in a separate function
    // (recommended: distance check + !is_spawned + !temp_tris.empty())
}
