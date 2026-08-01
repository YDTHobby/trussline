// Trussline Spike B - node/beam solver cost at 2 kHz.
//
// SCOPE, STATED UP FRONT. This is a faithful transcription of the two dominant
// loops of RoR's solver - Actor::CalcBeams and Actor::CalcNodes from
// ActorForcesEuler.cpp - operating on the real node_t / beam_t field layout from
// SimData.h. It is NOT the whole solver.
//
// Deliberately excluded: wheels, shocks2/3, hydros, commands, ties, ropes,
// triggers, engine/powertrain, aerodynamics, buoyancy, replay, and real terrain
// collision (ground contact is a flat-plane stub). Those add cost.
//
// So the number this produces is a LOWER BOUND on real per-step cost. If the
// lower bound already blows the budget, the answer is decisive. If it fits with
// room, that is encouraging but not yet a pass - Phase 4 profiles the real thing.
//
// Extracting the full solver means stubbing 21 includes and ~200 App:: singleton
// accesses (Engine, ScriptEngine, Terrain, Replay, SoundScriptManager...), which
// is Phase-2-sized work, not spike-sized.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <random>
#include <vector>

// ---------------------------------------------------------------------------
// Value types. Ogre::Vector3 is three bare floats (verified in the V-2 audit),
// so this is layout-identical to what the real solver operates on.
// ---------------------------------------------------------------------------
struct Vec3
{
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}

    Vec3  operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3  operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3  operator*(float s)       const { return Vec3(x * s, y * s, z * s); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s)       { x *= s; y *= s; z *= s; return *this; }

    float squaredLength()            const { return x * x + y * y + z * z; }
    float dotProduct(const Vec3& o)  const { return x * o.x + y * o.y + z * o.z; }
};

// ---------------------------------------------------------------------------
// ApproxMath, post-R-16 form: memcpy rather than strict-aliasing-violating
// casts. Matches what is committed to the fork.
// ---------------------------------------------------------------------------
static inline float bit_cast_to_float(const int32_t bits)
{
    float f; std::memcpy(&f, &bits, sizeof(f)); return f;
}
static inline int32_t bit_cast_to_int(const float f)
{
    int32_t b; std::memcpy(&b, &f, sizeof(b)); return b;
}

static inline float fast_invSqrt(const float v)
{
    int i = 0x5f3759df - (bit_cast_to_int(v) >> 1);
    float y = bit_cast_to_float(static_cast<int32_t>(i));
    y *= (1.5f - (0.5f * v * y * y));
    return y;
}

static inline float approx_sqrt(const float y)
{
    int i = ((bit_cast_to_int(y) - 1065353216) >> 1) + 1065353216;
    return bit_cast_to_float(static_cast<int32_t>(i));
}

// ---------------------------------------------------------------------------
// Field layout mirrors SimData.h's node_t / beam_t hot members. Cache behaviour
// is a large part of what is being measured, so the ordering matters.
// ---------------------------------------------------------------------------
struct Node
{
    Vec3  RelPosition;
    Vec3  AbsPosition;
    Vec3  Velocity;
    Vec3  Forces;
    float mass;
    bool  nd_immovable;
    bool  nd_no_ground_contact;
    bool  nd_has_ground_contact;
};

enum BeamBounded { BOUNDED_NONE = 0, SHOCK1, TRIGGER, SHOCK2, SHOCK3, SUPPORTBEAM, ROPE };

struct Beam
{
    Node* p1;
    Node* p2;
    float k;                     // spring
    float d;                     // damping
    float L;                     // rest length
    float minmaxposnegstress;
    float maxposstress;
    float maxnegstress;
    float strength;
    float stress;
    float plastic_coef;
    int   bounded;
    int   bm_type;
    bool  bm_disabled;
    bool  bm_inter_actor;
};

static const float PHYSICS_DT = 0.0005f;   // SimConstants.h:20 - 2 kHz
static const float MIN_BEAM_LENGTH = 0.00001f;

// ---------------------------------------------------------------------------
// Actor::CalcBeams - transcribed from ActorForcesEuler.cpp:1210-1466
// ---------------------------------------------------------------------------
static void CalcBeams(std::vector<Beam>& beams)
{
    const int n = static_cast<int>(beams.size());
    for (int i = 0; i < n; i++)
    {
        Beam& b = beams[i];
        if (b.bm_disabled || b.bm_inter_actor)
            continue;

        Vec3  dis    = b.p1->RelPosition - b.p2->RelPosition;
        float dislen = dis.squaredLength();
        float inverted_dislen = fast_invSqrt(dislen);

        dislen *= inverted_dislen;
        float difftoBeamL = dislen - b.L;

        float k = b.k;
        float d = b.d;

        float v = (b.p1->Velocity - b.p2->Velocity).dotProduct(dis) * inverted_dislen;

        if (b.bounded == ROPE && difftoBeamL < 0.0f)
        {
            k = 0.0f;
            d *= 0.1f;
        }

        float slen = -k * difftoBeamL - d * v;
        b.stress = slen;

        // Deformation test. Rarely taken in steady state, but the branch and the
        // compare are paid every beam every step, so they stay in.
        float len = std::abs(slen);
        if (len > b.minmaxposnegstress)
        {
            if (b.bm_type == 0 && b.bounded != SHOCK1 && k != 0.0f)
            {
                if (slen > b.maxposstress && difftoBeamL < 0.0f)
                {
                    float yield_length = b.maxposstress / k;
                    float deform = difftoBeamL + yield_length * (1.0f - b.plastic_coef);
                    float Lold = b.L;
                    b.L += deform;
                    b.L = (MIN_BEAM_LENGTH > b.L) ? MIN_BEAM_LENGTH : b.L;
                    slen = slen - (slen - b.maxposstress) * 0.5f;
                    if (b.L > 0.0f && Lold > b.L)
                    {
                        b.maxposstress *= Lold / b.L;
                        b.minmaxposnegstress = std::min(b.maxposstress, -b.maxnegstress);
                        b.minmaxposnegstress = std::min(b.minmaxposnegstress, b.strength);
                    }
                }
            }
        }

        Vec3 f = dis;
        f *= (slen * inverted_dislen);
        b.p1->Forces += f;
        b.p2->Forces -= f;
    }
}

// ---------------------------------------------------------------------------
// Actor::CalcNodes - transcribed from ActorForcesEuler.cpp:1602-1647.
// Ground contact is a flat-plane stub (ROADMAP 1.2 Step 5) - the real
// Collisions::groundCollision/nodeCollision are excluded, and they are not free.
// ---------------------------------------------------------------------------
static void CalcNodes(std::vector<Node>& nodes, const Vec3& origin, float gravity)
{
    const int n = static_cast<int>(nodes.size());
    for (int i = 0; i < n; i++)
    {
        Node& nd = nodes[i];

        if (!nd.nd_no_ground_contact)
        {
            // Flat ground at y = 0: cheap stand-in for terrain collision.
            if (nd.AbsPosition.y < 0.0f)
            {
                nd.nd_has_ground_contact = true;
                nd.Forces.y += -nd.AbsPosition.y * 100000.0f - nd.Velocity.y * 1000.0f;
            }
            else
            {
                nd.nd_has_ground_contact = false;
            }
        }

        if (!nd.nd_immovable)
        {
            nd.Velocity    += nd.Forces * (1.0f / nd.mass) * PHYSICS_DT;
            nd.RelPosition += nd.Velocity * PHYSICS_DT;
            nd.AbsPosition  = origin + nd.RelPosition;
        }

        nd.Forces = Vec3(0.0f, nd.mass * gravity, 0.0f);

        float approx_speed = approx_sqrt(nd.Velocity.squaredLength());
        if (approx_speed > 6860.0f)
        {
            nd.Velocity = Vec3(0, 0, 0);   // anti-explosion guard
        }
    }
}

// ---------------------------------------------------------------------------
// Build a structurally plausible vehicle: nodes on a lattice, beams triangulated
// between near neighbours, matching the node:beam ratios measured in real RoR
// content (DAF semi 79:315, Agora bus 151:675 - roughly 1:4).
// ---------------------------------------------------------------------------
static void BuildVehicle(int numNodes, int numBeams,
                         std::vector<Node>& nodes, std::vector<Beam>& beams)
{
    nodes.clear(); nodes.resize(numNodes);
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> jitter(-0.05f, 0.05f);

    const int perRow = 4;
    for (int i = 0; i < numNodes; i++)
    {
        const int row = i / perRow;
        const int col = i % perRow;
        nodes[i].RelPosition = Vec3(col * 0.8f + jitter(rng),
                                    1.0f + jitter(rng),
                                    row * 0.5f + jitter(rng));
        nodes[i].AbsPosition = nodes[i].RelPosition;
        nodes[i].Velocity    = Vec3(0, 0, 0);
        nodes[i].mass        = 20.0f;
        nodes[i].Forces      = Vec3(0, 20.0f * -9.81f, 0);
        nodes[i].nd_immovable = false;
        nodes[i].nd_no_ground_contact = false;
    }

    beams.clear(); beams.reserve(numBeams);
    std::uniform_int_distribution<int> span(1, 7);
    for (int i = 0; i < numBeams; i++)
    {
        Beam b{};
        const int a = i % numNodes;
        int c = (a + span(rng)) % numNodes;
        if (c == a) c = (a + 1) % numNodes;

        b.p1 = &nodes[a];
        b.p2 = &nodes[c];
        Vec3 d = b.p1->RelPosition - b.p2->RelPosition;
        b.L = std::sqrt(d.squaredLength());
        b.k = 9000000.0f;
        b.d = 12000.0f;
        b.maxposstress = 100000.0f;
        b.maxnegstress = -100000.0f;
        b.strength     = 100000.0f;
        b.minmaxposnegstress = 100000.0f;
        b.plastic_coef = 0.5f;
        b.bounded = BOUNDED_NONE;
        b.bm_type = 0;
        b.bm_disabled = false;
        b.bm_inter_actor = false;
        beams.push_back(b);
    }
}

struct Case { const char* name; int nodes; int beams; int actors; };

int main(int argc, char** argv)
{
    // Real counts from Trussline's license-clean content, plus heavier cases for
    // headroom. Wheels add nodes/beams procedurally at spawn, so shipped truck
    // files understate runtime totals - hence the larger cases.
    const Case cases[] = {
        { "DAF semi (real: 79n/315b)",      79,  315, 1 },
        { "Agora bus (real: 151n/675b)",   151,  675, 1 },
        { "typical w/ wheels (est.)",      220, 1100, 1 },
        { "heavy vehicle (est.)",          400, 2000, 1 },
        { "4x DAF semi",                    79,  315, 4 },
    };

    const int stepsPerSecond = 2000;                 // PHYSICS_DT = 0.0005
    const int benchSteps     = (argc > 1) ? std::atoi(argv[1]) : 20000;  // 10 s of sim

    std::printf("Trussline Spike B - node/beam solver @ %d Hz\n", stepsPerSecond);
    std::printf("steps per case: %d (%.1f s of simulated time)\n\n",
                benchSteps, double(benchSteps) / stepsPerSecond);
    std::printf("%-32s %6s %6s %10s %10s %9s\n",
                "case", "nodes", "beams", "us/step", "budget%", "verdict");
    std::printf("%s\n", "-------------------------------------------------------------------------------");

    for (const Case& c : cases)
    {
        std::vector<std::vector<Node>> allNodes(c.actors);
        std::vector<std::vector<Beam>> allBeams(c.actors);
        for (int a = 0; a < c.actors; a++)
            BuildVehicle(c.nodes, c.beams, allNodes[a], allBeams[a]);

        const Vec3 origin(0, 0, 0);
        const float gravity = -9.81f;

        // Warm caches / let the structure settle before timing.
        for (int s = 0; s < 500; s++)
            for (int a = 0; a < c.actors; a++)
            {
                CalcBeams(allBeams[a]);
                CalcNodes(allNodes[a], origin, gravity);
            }

        const auto t0 = std::chrono::steady_clock::now();
        for (int s = 0; s < benchSteps; s++)
            for (int a = 0; a < c.actors; a++)
            {
                CalcBeams(allBeams[a]);
                CalcNodes(allNodes[a], origin, gravity);
            }
        const auto t1 = std::chrono::steady_clock::now();

        const double totalUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
        const double usPerStep = totalUs / benchSteps;

        // One core's real-time budget for a 2 kHz step is 500 us.
        const double budgetPct = (usPerStep / 500.0) * 100.0;
        const char* verdict = (budgetPct < 30.0) ? "PASS"
                            : (budgetPct < 100.0) ? "tight" : "FAIL";

        std::printf("%-32s %6d %6d %10.2f %9.1f%% %9s\n",
                    c.name, c.nodes * c.actors, c.beams * c.actors,
                    usPerStep, budgetPct, verdict);
    }

    std::printf("\nBudget%% = fraction of one core's 500 us real-time window per 2 kHz step.\n");
    std::printf("ROADMAP Gate 1 threshold: one representative vehicle under ~30%%.\n");
    std::printf("LOWER BOUND - excludes wheels, shocks, hydros, engine, aero, real collision.\n");
    return 0;
}
