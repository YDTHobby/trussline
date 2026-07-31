/*
 * Trussline — node/beam solver C API (SKETCH)
 * ============================================
 *
 * STATUS: design sketch, not compiled. Written against the 2026-07-31 source
 * audit (DECISIONS.md V-2). Refine during ROADMAP § 1.3, before Spike B — the
 * Spike B harness must consume the solver *through this header*, because that
 * is what proves the boundary is real rather than aspirational.
 *
 * WHY THIS EXISTS (D-009): the solver is the one asset that survives any
 * strategy pivot. If Gate 1 sends us to Godot, this header is the seam the
 * GDExtension binds to. Boundary erosion is a bug, not a shortcut.
 *
 * WHAT THE AUDIT MAKES EASY:
 *   Ogre::Vector3 is three bare floats and Ogre::Real is float. node_t is
 *   memset-POD by its own constructor (SimData.h:263). So the POD types below
 *   are layout-compatible with the existing structs — swapping them in is a
 *   compile-time change with zero binary layout impact and no serialization
 *   shim. Start from the existing source/main/utils/Vec3.h.
 *
 * WHAT THE AUDIT MAKES HARD:
 *   Not OGRE — the App:: global singletons. Actor.cpp alone has 109 App::
 *   accesses. Everything ambient (console, CVars, audio, terrain, timers) must
 *   be injected through tl_host_callbacks rather than reached for.
 *
 * C linkage, opaque handles, no C++ types across the boundary — so a Godot
 * GDExtension, a JNI shim, or a headless benchmark can all bind to it.
 */

#ifndef TRUSSLINE_SOLVER_H
#define TRUSSLINE_SOLVER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Value types -------------------------------------------------------- */
/* Layout-compatible with Ogre::Vector3 / Ogre::Real by construction. Do not
 * add members, change order, or introduce alignment attributes. */

typedef struct { float x, y, z; } tl_vec3;

/* The fixed simulation timestep, 0.5 ms / 2 kHz. Currently a #define at
 * SimConstants.h:20 used at ~70 sites; make it constexpr during extraction.
 * This is semantics, not a tuning knob (AGENTS.md hard constraint #4). */
#define TL_PHYSICS_DT 0.0005f

/* --- Opaque handles ----------------------------------------------------- */

typedef struct tl_world  tl_world;   /* simulation universe: terrain + actors */
typedef struct tl_actor  tl_actor;   /* one vehicle (node/beam network)       */

typedef uint32_t tl_actor_id;
#define TL_INVALID_ACTOR ((tl_actor_id)0)

typedef enum {
    TL_OK = 0,
    TL_ERR_INVALID_ARG,
    TL_ERR_OUT_OF_MEMORY,
    TL_ERR_BAD_SCENE_DATA,
    TL_ERR_ACTOR_LIMIT,
    TL_ERR_UNSUPPORTED     /* e.g. node/beam count over the device-tier cap */
} tl_result;

/* --- Host callbacks ----------------------------------------------------- */
/*
 * Everything the solver currently reaches for via App:: globals gets injected
 * here instead. Any NULL member must be safe: the solver falls back to an
 * inert default (silent log, flat ground, no audio). That fallback behavior is
 * what makes the headless Spike B harness cheap to write.
 */
typedef struct {
    void *user;

    /* Logging — replaces App::GetConsole()->putMessage / RoR::LogFormat. */
    void (*log)(void *user, int level, const char *msg);

    /* Terrain query — replaces GetTerrain()->GetCollisions(). Returning a
     * constant height gives the flat-plane stub recommended for first arm64
     * bring-up (ROADMAP § 1.2 Step 5). */
    float (*ground_height)(void *user, float x, float z);

    /* Environment. NULL => standard gravity, no water. */
    tl_vec3 (*gravity)(void *user);
    float   (*water_height)(void *user);

    /* Sound events. NULL => silent; SOUND_* already no-op without USE_OPENAL,
     * so the headless harness gets this for free. */
    void (*sound_event)(void *user, tl_actor_id actor, int event_id, float value);
} tl_host_callbacks;

/* --- World lifecycle ---------------------------------------------------- */

typedef struct {
    const tl_host_callbacks *host;
    uint32_t max_actors;          /* device-tier cap; 0 => library default   */
    uint32_t max_nodes_per_actor; /* complexity budget, enforced on load     */
    uint32_t max_beams_per_actor;
    int      deterministic;       /* 1 => disable fast-math-sensitive paths
                                   *      and any nondeterministic ordering,
                                   *      for the R-17 reference baseline    */
} tl_world_desc;

tl_world *tl_world_create(const tl_world_desc *desc, tl_result *out_err);
void      tl_world_destroy(tl_world *w);

/* --- Actor lifecycle ---------------------------------------------------- */
/*
 * NOTE: deliberately NOT a truck-file parser. ActorSpawner.cpp (8,146 LOC) is
 * excluded from the solver — it is parsing tangled through OGRE meshes,
 * materials, and the resource system. Parsing stays on the host side; the
 * solver receives flat node/beam arrays. This single split is what makes the
 * extraction tractable (ROADMAP § 1.2 Step 3).
 */
typedef struct {
    tl_vec3  position;
    float    mass;
    uint32_t flags;      /* contactless, loadbearing, buoyant, ... */
} tl_node_desc;

typedef struct {
    uint32_t node_a, node_b;
    float    spring;     /* beam_t::k */
    float    damping;    /* beam_t::d */
    float    length;     /* beam_t::L */
    uint32_t flags;      /* rope, support, hydro, shock, ... */
} tl_beam_desc;

typedef struct {
    const tl_node_desc *nodes;  size_t node_count;
    const tl_beam_desc *beams;  size_t beam_count;
    tl_vec3 spawn_position;
    float   spawn_rotation_y;
} tl_actor_desc;

tl_actor_id tl_actor_spawn(tl_world *w, const tl_actor_desc *desc, tl_result *out_err);
tl_result   tl_actor_remove(tl_world *w, tl_actor_id id);

/* --- Simulation --------------------------------------------------------- */
/*
 * Advances by whole TL_PHYSICS_DT substeps, carrying the remainder internally
 * (mirrors the existing accumulator at ActorManager.cpp:1113-1123).
 *
 * max_substeps is NOT optional on mobile: the current code has no upper clamp,
 * and an unbounded substep burst after an OS-induced long frame is a real
 * lifecycle hazard (ROADMAP § 4.3). Pass a hard ceiling and drop the surplus.
 * out_substeps_run reports what actually executed so the caller can detect it.
 */
tl_result tl_world_step(tl_world *w,
                        float     wall_dt_seconds,
                        uint32_t  max_substeps,
                        uint32_t *out_substeps_run);

/* --- Input -------------------------------------------------------------- */

typedef enum {
    TL_CTRL_THROTTLE = 0,
    TL_CTRL_BRAKE,
    TL_CTRL_STEER,          /* -1 .. +1 */
    TL_CTRL_CLUTCH,
    TL_CTRL_HANDBRAKE,
    TL_CTRL_HYDRO,          /* generic articulated-machinery axis */
    TL_CTRL_COUNT
} tl_control;

tl_result tl_actor_set_control(tl_world *w, tl_actor_id id, tl_control c, float value);
tl_result tl_actor_set_gear(tl_world *w, tl_actor_id id, int gear);

/* --- State readback ----------------------------------------------------- */
/*
 * Bulk copy-out, never pointers into solver memory: the caller may be on the
 * render thread reading while the sim thread runs. This is the sim->render
 * handoff boundary, and it is where thread-safety review concentrates
 * (ROADMAP § 4.1).
 */
tl_result tl_actor_read_nodes(const tl_world *w, tl_actor_id id,
                              tl_vec3 *out_positions, size_t capacity,
                              size_t *out_written);

typedef struct {
    tl_vec3 position;        /* center of mass    */
    tl_vec3 velocity;
    float   engine_rpm;
    float   wheel_speed;
    int     gear;
    uint32_t node_count, beam_count;
    uint32_t broken_beam_count;
} tl_actor_state;

tl_result tl_actor_read_state(const tl_world *w, tl_actor_id id, tl_actor_state *out);

/* --- Introspection ------------------------------------------------------ */

typedef struct {
    double   last_step_ms;
    double   avg_step_ms;
    uint32_t active_actors;
    uint32_t total_nodes, total_beams;
} tl_perf_stats;

void tl_world_get_perf(const tl_world *w, tl_perf_stats *out);

/* Build provenance — must report whether fast-math/FP-contract were enabled,
 * because "which binary produced this trajectory" is otherwise unanswerable
 * when comparing ARM against the desktop reference (R-17). */
const char *tl_build_info(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TRUSSLINE_SOLVER_H */
