#include "mario1.h"
#include "../common/Ptr.h"
#include "../common/Kscheme.h"
#include "game/kernel/jak1/kscheme.h"
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



void MarioManager::SpawnActorMesh(const std::string& actor_name) {
  // Look up the actor in our map
  auto it = m_actor_infos.find(actor_name);
  if (it == m_actor_infos.end()) {
    printf("[MarioMgr] Spawn failed: Actor '%s' not found in m_actor_infos\n",
           actor_name.c_str());
    return;
  }

  ActorInfo& info = it->second;

  // Skip if already spawned
  if (info.is_spawned) {
    printf("[MarioMgr] Spawn skipped: Actor '%s' already spawned (ID %u)\n",
           actor_name.c_str(), info.sm64_id);
    return;
  }

  // Safety: no triangles = nothing to spawn
  if (info.temp_tris.empty()) {
    printf("[MarioMgr] Spawn skipped: Actor '%s' has no triangles in temp_tris\n",
           actor_name.c_str());
    return;
  }

  // Build the SM64SurfaceObject struct that libsm64 expects
  SM64SurfaceObject obj{};

  // Position (already in SM64 units / meters)
  obj.transform.position[0] = info.pos[0];
  obj.transform.position[1] = info.pos[1];
  obj.transform.position[2] = info.pos[2];

  // Rotation (assuming your euler_rot is in radians — if degrees, convert here)
  obj.transform.eulerRotation[0] = info.euler_rot[0];
  obj.transform.eulerRotation[1] = info.euler_rot[1];
  obj.transform.eulerRotation[2] = info.euler_rot[2];

  // Triangle data — directly from temp_tris
  obj.surfaces     = info.temp_tris.data();
  obj.surfaceCount = static_cast<uint32_t>(info.temp_tris.size());

  printf("[MarioMgr] Spawning '%s' — %u triangles at pos (%.1f, %.1f, %.1f)\n",
         actor_name.c_str(), obj.surfaceCount,
         obj.transform.position[0], obj.transform.position[1], obj.transform.position[2]);

  // Create the surface object in libsm64
  uint32_t sm64_id = sm64_surface_object_create(&obj);

  if (sm64_id != 0) {
    info.sm64_id = sm64_id;
    info.is_spawned = true;
    printf("[MarioMgr] SUCCESS: Spawned sm64 object ID %u for '%s' (%u tris)\n",
           sm64_id, actor_name.c_str(), obj.surfaceCount);

    // Optional: free temp memory now that it's spawned
    // info.temp_tris.clear();
    // info.temp_tris.shrink_to_fit();
  } else {
    info.is_spawned = false;
    printf("[MarioMgr] FAILED to spawn sm64 object for '%s' — check libsm64\n",
           actor_name.c_str());
  }
}

void MarioManager::AddOrUpdateTrisToTempBuffer(const triangle_package* pkg) {
  // This takes in a point from the GOAL side that we just updated, and processes the data so we can store it on the cpp side

  //First lets make sure the pointer is valid, no idea if this works and dont care because maybe it works
  if (!pkg) {
    printf("[MarioMgr] ERROR: null package pointer\n");
    return;
  }


  // Get actor name from GOAL pointer (assuming Ptr<String> is your safe GOAL string accessor)
  std::string actor_name = Ptr<String>(pkg->actor_name).c()->data();
  if (actor_name.empty()) {
    printf("[MarioMgr] ERROR: empty actor name in package\n");
    std::abort();
    return;
  }

  // Find or create actor entry
  auto [actor_it, inserted] = m_actor_infos.try_emplace(actor_name);
  ActorInfo& info = actor_it->second;

  if (inserted) {
    info.name = actor_name;
    printf("[MarioMgr] Created new temp actor: %s\n", actor_name.c_str());
  }

  // ─────────────────────────────────────────────────────────────
  // Early-out: if we already have all (or more) triangles expected
  //            then skip this entire package
  // ─────────────────────────────────────────────────────────────
  const size_t expected_tris = static_cast<size_t>(pkg->tri_count);
  if (info.temp_tris.size() >= expected_tris) {
    // We should print when this happens but not doing it to avoid lag, we need to revist this at some point, at the moment we just "skip" any actor that we have seen all the triangles for, but thats not really going to work for ever.
    // printf("[MarioMgr] Skipping package for %s — already have %zu / %zu tris\n",
    //        actor_name.c_str(), info.temp_tris.size(), expected_tris);
    return;
  }

  printf("\n[MarioMgr] Processing package for %s (tris so far: %zu / expected %zu)\n",
         actor_name.c_str(), info.temp_tris.size(), expected_tris);


  // Prepare surface
  SM64Surface surf{};
  surf.type = SURFACE_DEFAULT;
  surf.force = 0;
  surf.terrain = TERRAIN_STONE;

  bool valid = true;

  for (int v = 0; v < 3; ++v) {
    const auto& vec = pkg->tris[v];

    if (!std::isfinite(vec.x) || !std::isfinite(vec.y) || !std::isfinite(vec.z)) {
      valid = false;
      printf("[MarioMgr] Invalid vertex %d: (%.3f, %.3f, %.3f) in %s\n", v, vec.x, vec.y, vec.z,
             actor_name.c_str());
             std::abort();
      break;
    }

    // here we adjust the units from jak cords to mario cords this is untested and hopefully the meshes just work rn but if they dont invesitgate this more
    float xf = vec.x * METERS_TO_UNITS;
    float yf = vec.y * METERS_TO_UNITS;
    float zf = vec.z * METERS_TO_UNITS;

    surf.vertices[v][0] = static_cast<int32_t>(std::round(xf));
    surf.vertices[v][1] = static_cast<int32_t>(std::round(yf));
    surf.vertices[v][2] = static_cast<int32_t>(std::round(zf));
  }

  if (valid) {
    info.temp_tris.push_back(surf);
    printf("[MarioMgr] Added temp tri #%zu for %s → v0(%d,%d,%d) v1(%d,%d,%d) v2(%d,%d,%d)\n",
           info.temp_tris.size(), actor_name.c_str(), surf.vertices[0][0], surf.vertices[0][1],
           surf.vertices[0][2], surf.vertices[1][0], surf.vertices[1][1], surf.vertices[1][2],
           surf.vertices[2][0], surf.vertices[2][1], surf.vertices[2][2]);
  } else {
    printf("[MarioMgr] Ignored invalid/NaN triangle for %s\n", actor_name.c_str());
  }

  // ─────────────────────────────────────────────────────────────
  // Completion check — only log when we think we're done
  // ─────────────────────────────────────────────────────────────
  bool is_complete = (pkg->tri_count == pkg->tri_index) ||
                     (info.temp_tris.size() >= expected_tris);

  if (is_complete) {
    printf(
        "[MarioMgr] Completed triangle set for %s — total tris in temp buffer: %zu (expected %zu)\n",
        actor_name.c_str(), info.temp_tris.size(), expected_tris);

    // Only spawn if we haven't already done it
    if (info.is_spawned) {
      printf("[MarioMgr] Actor %s already spawned — skipping duplicate spawn\n",
             actor_name.c_str());
    } else {
      // Mark as spawned
      

      // Call the spawn function (placeholder — implement this!)
      SpawnActorMesh(actor_name);
      info.is_spawned = true;

      printf("[MarioMgr] Spawned mesh for actor %s (ID: %u, %zu tris)\n", actor_name.c_str(),
             info.sm64_id, info.temp_tris.size());
      printf("[MarioMgr] Spawned mesh for actor %s (ID: %u, %zu tris)\n", actor_name.c_str(),
             info.sm64_id, info.temp_tris.size());

      // Optional: clear temp data after successful spawn to save memory
      // info.temp_tris.clear();
      // info.temp_tris.shrink_to_fit();
    }
  }
}

// void MarioManager::AddOrUpdateTrisToTempBuffer(
//     uint32_t triangle_package_pointer)
// {

  
//     // char* name = Ptr<String>(jak1::intern_from_c("*curr-actor-name-str*")).c()->data();
//     //char* name = Ptr<String>(jak1::intern_from_c("*curr-actor-name-str*")).c()->data();
//     // Optional: see what GOAL is actually sending
//     //  printf("[AddTris] name=%s   bits: x=%08x y=%08x z=%08x idx=%08x\n",
//     //         name ? name : "(null)", x_bits, y_bits, z_bits, vert_index_bits);
    
//     // ─────────────────────────────────────────────────────────────
//     // Decode bit-packed floats (same as your working version)
//     // ─────────────────────────────────────────────────────────────
//     float x_f, y_f, z_f, vert_index_f, name_f;
//     memcpy(&x_f, &x_bits, sizeof(float));
//     memcpy(&y_f, &y_bits, sizeof(float));
//     memcpy(&z_f, &z_bits, sizeof(float));
//     memcpy(&vert_index_f, &vert_index_bits, sizeof(float));

//     memcpy(&name_f, &name_bits, sizeof(float));



//     x_f *= METERS_TO_UNITS;
//     y_f *= METERS_TO_UNITS;
//     z_f *= METERS_TO_UNITS;

//     int vert_index = static_cast<int>(vert_index_f + 0.5f);

//     if (vert_index < 1 || vert_index > 3) {
//         // printf("[MarioMgr] ERROR: invalid vert_index %d (must be 1-3) for '%s'\n",
//         //        vert_index, name ? name : "?");
//         return;
//     }

//     // ─────────────────────────────────────────────────────────────
//     // Find or create actor
//     // ─────────────────────────────────────────────────────────────
//     std::string actor_name = "shithead";//name;  // name_ptr ? name_ptr : "unnamed";

//     auto [it, inserted] = m_actor_infos.try_emplace(actor_name);
//     ActorInfo& info = it->second;

//     if (inserted) {
//         info.name         = actor_name;
//         info.pos[0]       = 0.0f;
//         info.pos[1]       = 0.0f;
//         info.pos[2]       = 0.0f;
//         info.euler_rot[0] = 0.0f;
//         info.euler_rot[1] = 0.0f;
//         info.euler_rot[2] = 0.0f;
//         info.is_platform  = false;
//         info.is_spawned   = false;
//         info.sm64_id      = 0;
//         info.mesh         = nullptr;
//         info.num_surfaces = 0;
//         info.mesh_hash    = 0;
//         // vectors are already empty
//         printf("[MarioMgr] New actor created: '%s'\n", actor_name.c_str());
//     }

//     // ─────────────────────────────────────────────────────────────
//     // Append the new vertex (x,y,z as floats)
//     // ─────────────────────────────────────────────────────────────
//     info.vertex_accum.push_back(x_f);
//     info.vertex_accum.push_back(y_f);
//     info.vertex_accum.push_back(z_f);

//     // printf("[MarioMgr] Added vertex %d → accum size now %zu\n",
//     //        vert_index, info.vertex_accum.size());

//     // ─────────────────────────────────────────────────────────────
//     // If we have at least 9 floats (3 full vertices), try to commit
//     // ─────────────────────────────────────────────────────────────
//     size_t n = info.vertex_accum.size();
//     if (n >= 9) {
//         // Take the last 9 floats (last 3 vertices)
//         size_t base = n - 9;

//         SM64Surface surf{};
//         surf.type    = SURFACE_DEFAULT;   // or your preferred default
//         surf.force   = 0;
//         surf.terrain = TERRAIN_STONE;

//         // Convert last 3 vertices to int32_t
//         for (int v = 0; v < 3; ++v) {
//             size_t off = base + v * 3;
//             surf.vertices[v][0] = static_cast<int32_t>(std::round(info.vertex_accum[off + 0]));
//             surf.vertices[v][1] = static_cast<int32_t>(std::round(info.vertex_accum[off + 1]));
//             surf.vertices[v][2] = static_cast<int32_t>(std::round(info.vertex_accum[off + 2]));
//         }

//         info.temp_tris.push_back(surf);

//         printf("[MarioMgr] Committed tri #%zu for '%s'   v0=(%d,%d,%d) v1=(%d,%d,%d) v2=(%d,%d,%d)\n",
//                info.temp_tris.size(),
//                actor_name.c_str(),
//                surf.vertices[0][0], surf.vertices[0][1], surf.vertices[0][2],
//                surf.vertices[1][0], surf.vertices[1][1], surf.vertices[1][2],
//                surf.vertices[2][0], surf.vertices[2][1], surf.vertices[2][2]);

//         // Remove the committed 9 floats so accum stays clean for next triangle
//         info.vertex_accum.erase(info.vertex_accum.begin(), info.vertex_accum.begin() + base + 9);
//     }
// }


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
