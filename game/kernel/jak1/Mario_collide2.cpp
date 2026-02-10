// // Mario_collide2.cpp
// #include "Mario_collide2.h"
// #include <cstdio>
// #include <cstring>
// #include <cmath>
// #include <algorithm>

// // Globals
// bool g_run_mario_collide = true;
// std::unordered_map<std::string, ActorCollisionData> g_active_actors;

// // Simple FNV-1a hash to detect mesh changes
// static uint32_t simple_mesh_hash(const SM64Surface* surfaces, uint32_t count) {
//     uint32_t hash = 2166136261u;
//     for (uint32_t i = 0; i < count; ++i) {
//         for (int v = 0; v < 3; ++v) {
//             for (int c = 0; c < 3; ++c) {
//                 uint32_t val = static_cast<uint32_t>(surfaces[i].vertices[v][c]);
//                 hash ^= val;
//                 hash *= 16777619u;
//             }
//         }
//     }
//     return hash;
// }

// // Cleanup distance (meters)
// static constexpr float CLEANUP_RADIUS_SQ = 40.0f * 40.0f;

// // Main collision system update — call this every frame
// void update_mario_actor_collide_system() {
//     if (!g_run_mario_collide) {
//         // printf("[MarioCollide] Paused (*run-mario-collide* = #f)\n");
//         return;
//     }

//     // 2. Remove far-away actors
//     std::vector<std::string> to_remove;
//     for (const auto& [name, actor] : g_active_actors) {
//         float dx = actor.root_trans[0] - g_mario_state.position[0];
//         float dz = actor.root_trans[2] - g_mario_state.position[2];
//         if (dx*dx + dz*dz > CLEANUP_RADIUS_SQ) {
//             to_remove.push_back(name);
//         }
//     }

//     for (const auto& name : to_remove) {
//         auto it = g_active_actors.find(name);
//         if (it != g_active_actors.end()) {
//             if (it->second.sm64_id != 0) {
//                 sm64_surface_object_delete(it->second.sm64_id);
//                 printf("[MarioCollide] Removed distant '%s' (id=%u)\n",
//                        name.c_str(), it->second.sm64_id);
//             }
//             if (it->second.surfaces) free(it->second.surfaces);
//             g_active_actors.erase(it);
//         }
//     }

//     // 3–7. Process actors
//     for (auto& [name, actor] : g_active_actors) {
//         // printf("[MarioCollide] Processing '%s' (%u tris)\n", name.c_str(), actor.num_tris);

//         // Skip special cases (customize later)
//         if (name.find("periscope") != std::string::npos) {
//             // printf("  → skipping (periscope)\n");
//             continue;
//         }

//         // 5. Already spawned + mesh unchanged → just move
//         if (actor.is_spawned && actor.sm64_id != 0) {
//             uint32_t current_hash = simple_mesh_hash(actor.surfaces, actor.num_tris);
//             if (current_hash == actor.mesh_hash) {
//                 SM64ObjectTransform xf{};
//                 xf.position[0] = actor.root_trans[0];
//                 xf.position[1] = actor.root_trans[1];
//                 xf.position[2] = actor.root_trans[2];
//                 xf.eulerRotation[1] = actor.rotation[1];  // yaw

//                 sm64_surface_object_move(actor.sm64_id, &xf);

//                 // printf("  → moved '%s' to (%.2f, %.2f, %.2f)\n", name.c_str(),
//                 //        actor.root_trans[0], actor.root_trans[1], actor.root_trans[2]);
//                 continue;
//             } else {
//                 printf("  → mesh changed '%s' → respawning\n", name.c_str());
//                 sm64_surface_object_delete(actor.sm64_id);
//                 actor.is_spawned = false;
//                 actor.sm64_id = 0;
//             }
//         }

//         // 6+7. Spawn / respawn
//         if (actor.num_tris == 0 || !actor.surfaces) {
//             // printf("  → no triangles for '%s'\n", name.c_str());
//             continue;
//         }

//         SM64SurfaceObject obj{};
//         memset(&obj, 0, sizeof(obj));

//         obj.transform.position[0] = actor.root_trans[0];
//         obj.transform.position[1] = actor.root_trans[1];
//         obj.transform.position[2] = actor.root_trans[2];
//         obj.transform.eulerRotation[1] = actor.rotation[1];

//         obj.surfaceCount = actor.num_tris;
//         obj.surfaces = actor.surfaces;

//         uint32_t id = sm64_surface_object_create(&obj);

//         if (id != 0) {
//             actor.sm64_id = id;
//             actor.is_spawned = true;
//             actor.mesh_hash = simple_mesh_hash(actor.surfaces, actor.num_tris);

//             printf("[MarioCollide] SPAWNED '%s' (id=%u, %u tris, active=%zu)\n",
//                    name.c_str(), id, actor.num_tris, g_active_actors.size());
//         } else {
//             printf("[MarioCollide] FAILED to spawn '%s'\n", name.c_str());
//         }
//     }
// }

// // Pseudo-floor under Mario (fixed surface)
// void update_psuedo_floor_under_mario() {
//     const char* floorName = "psuedo-floor";

//     pc_begin_actor_collide(floorName, true);  // is platform

//     // Assuming psuedo_floor_surfaces is a global array of SM64Surface
//     // Adjust this loop if it's defined differently (count, pointer, etc.)
//     extern SM64Surface psuedo_floor_surfaces[];  // declare wherever it lives
//     extern int psuedo_floor_surfaces_count;

//     for (int i = 0; i < psuedo_floor_surfaces_count; ++i) {
//         const auto& tri = psuedo_floor_surfaces[i];
//         pc_add_triangle_to_current_actor(
//             tri.vertices[0],
//             tri.vertices[1],
//             tri.vertices[2]
//         );
//     }

//     // Place it under Mario, y-offset by -300 units (adjust as needed)
//     pc_finish_actor_collide(
//         floorName,
//         g_mario_state.position[0],
//         g_mario_state.position[1] - 300.0f,
//         g_mario_state.position[2],
//         0.0f  // no yaw rotation
//     );
// }

// // Platform info from GOAL (called from pc_update_platform_info_from_goal)
// void update_platform_info_from_goal(u32 platform_info_ptr) {
//     if (!platform_info_ptr) return;

//     PlatformInfo* info = (PlatformInfo*)platform_info_ptr;

//     // Update global platform info
//     g_platform_info = *info;
//     g_platform_info_valid = true;

//     // printf("[Platform] Updated from GOAL: x=%u y=%u z=%u  rot=%u %u %u %u\n",
//     //        info->x_pos, info->y_pos, info->z_pos,
//     //        info->rot_x, info->rot_y, info->rot_z, info->rot_w);
// }

// // Moving platform update (if you have a spinning/sliding platform)
// void update_moving_platform() {
//     // Example: simple back-and-forth motion
//     // You can expand this with real GOAL data later
//     static float timer = 0.0f;
//     timer += 1.0f / 60.0f;

//     float phase = sinf(timer * 1.0f) * 500.0f;  // ±500 units

//     if (g_active_actors.count("moving-platform")) {
//         auto& plat = g_active_actors["moving-platform"];
//         plat.root_trans[0] += phase * 0.016f;  // small adjustment per frame
//         // or set directly from GOAL if you have real data
//     }
// }

// // Platform getters (used from kmachine.cpp)
// uint64_t pc_get_platform_x() { return g_platform_info.x_pos; }
// uint64_t pc_get_platform_y() { return g_platform_info.y_pos; }
// uint64_t pc_get_platform_z() { return g_platform_info.z_pos; }

// // GOAL → C++ API
// void pc_begin_actor_collide(const char* name_cstr, bool is_platform) {
//     std::string name = name_cstr ? name_cstr : "unnamed";

//     auto& actor = g_active_actors[name];
//     actor.name = name;
//     actor.is_platform = is_platform;

//     // Clear previous data if re-sending
//     if (actor.surfaces) {
//         free(actor.surfaces);
//         actor.surfaces = nullptr;
//     }
//     actor.num_tris = 0;
//     actor.is_spawned = false;
//     actor.sm64_id = 0;

//     // printf("[Collide] Begin '%s' (platform=%d)\n", name.c_str(), is_platform);
// }

// void pc_add_triangle_to_current_actor(int32_t v0[3], int32_t v1[3], int32_t v2[3]) {
//     if (g_active_actors.empty()) return;
//     auto& actor = g_active_actors.rbegin()->second;  // last one

//     // Grow buffer in chunks
//     if (actor.num_tris % 64 == 0) {
//         size_t new_size = (actor.num_tris + 64) * sizeof(SM64Surface);
//         actor.surfaces = (SM64Surface*)realloc(actor.surfaces, new_size);
//     }

//     SM64Surface& s = actor.surfaces[actor.num_tris++];
//     s.type = 0;   // SURFACE_DEFAULT or whatever you use
//     s.force = 0;

//     // Copy vertices (libsm64 uses int32_t in newer versions, adjust if yours is int16_t)
//     for (int i = 0; i < 3; ++i) {
//         s.vertices[0][i] = v0[i];
//         s.vertices[1][i] = v1[i];
//         s.vertices[2][i] = v2[i];
//     }
// }

// void pc_finish_actor_collide(const char* name_cstr, float x, float y, float z, float rot_y) {
//     std::string name = name_cstr ? name_cstr : "unnamed";
//     auto it = g_active_actors.find(name);
//     if (it == g_active_actors.end()) return;

//     auto& actor = it->second;
//     actor.root_trans[0] = x;
//     actor.root_trans[1] = y;
//     actor.root_trans[2] = z;
//     actor.rotation[1] = rot_y;

//     // printf("[Collide] Finished '%s' @ (%.2f, %.2f, %.2f) yaw=%.1f  tris=%u\n",
//     //        name.c_str(), x, y, z, rot_y, actor.num_tris);
// }

// void pc_remove_actor_collide(const char* name_cstr) {
//     if (!name_cstr) return;
//     std::string name = name_cstr;

//     auto it = g_active_actors.find(name);
//     if (it != g_active_actors.end()) {
//         if (it->second.sm64_id) {
//             sm64_surface_object_delete(it->second.sm64_id);
//         }
//         if (it->second.surfaces) {
//             free(it->second.surfaces);
//         }
//         g_active_actors.erase(it);
//         printf("[MarioCollide] Removed '%s'\n", name.c_str());
//     }
// }