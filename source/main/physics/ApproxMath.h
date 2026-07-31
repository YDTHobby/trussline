/*
    This source file is part of Rigs of Rods
    Copyright 2009 Lefteris Stamatogiannakis

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

/// @file
/// @author estama
/// @date   April 6, 2009, 2:57 AM

#pragma once

#include "Application.h"

#include <cstdint>
#include <cstring>

/// PRNG state for frand()/frand_02()/frand_11().
///
/// thread_local, not plain static: these are called from inside the
/// per-actor force tasks that ActorManager dispatches across the thread pool,
/// so a shared counter is a data race on the solver's hot path. ARM's weaker
/// memory model would interleave it differently from x86, which is a poor
/// property for a physics comparison to depend on.
///
/// unsigned, not int: the `mirand *= 16807` below overflows by design, and
/// signed overflow is undefined behaviour. Unsigned wraparound is defined and
/// produces the identical bit pattern, so the generated sequence is unchanged.
static thread_local uint32_t mirand = 1;

/// Reinterpret a bit pattern as float, and back.
///
/// Do NOT "simplify" these back to `*(float*)&i`. That form is a strict-aliasing
/// violation, and GCC/Clang exploit strict aliasing far more aggressively than
/// MSVC does — on aarch64 the old form was a latent miscompile on the solver's
/// hottest path. std::bit_cast is the modern spelling, but this project builds
/// as C++11; memcpy is the portable equivalent and optimises to the same
/// instruction.
inline float bit_cast_to_float(const int32_t bits)
{
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

inline float bit_cast_to_float(const uint32_t bits)
{
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

inline int32_t bit_cast_to_int(const float f)
{
    int32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    return bits;
}

// Returns a random number in the range [0, 1]
inline float frand()
{
    mirand *= 16807;

    const uint32_t a = (mirand & 0x007fffffu) | 0x40000000u;

    return (bit_cast_to_float(a) - 2.0f) * 0.5f;
}

// Returns a random number in the range [0, 2]
inline float frand_02()
{
    mirand *= 16807;

    const uint32_t a = (mirand & 0x007fffffu) | 0x40000000u;

    return bit_cast_to_float(a) - 2.0f;
}

// Returns a random number in the range [-1, 1]
inline float frand_11()
{
    mirand *= 16807;

    const uint32_t a = (mirand & 0x007fffffu) | 0x40000000u;

    return bit_cast_to_float(a) - 3.0f;
}

// Calculates approximate e^x.
// Use it in code not requiring precision
inline float approx_exp(const float x)
{
    if (x < -15)
        return 0.f ;
    else if (x > 88)
        return 1e38f ;
    else {
        int i=12102203*x+1064652319;
        return bit_cast_to_float(static_cast<int32_t>(i));
    }
}

// Calculates approximate 2^x
// Use it in code not requiring precision
inline float approx_pow2(const float x)
{
    int i = 8388608*x+1065353216;

    return bit_cast_to_float(static_cast<int32_t>(i));
}

// Calculates approximate x^y
// Use it in code not requiring precision
inline float approx_pow(const float x, const float y)
{
    int i = y * (bit_cast_to_int(x) - 1065353216) + 1065353216;

    return bit_cast_to_float(static_cast<int32_t>(i));
}

// Calculates approximate square_root(x)
// Use it in code not requiring precision
inline float approx_sqrt(const float y)
{
    int i = ((bit_cast_to_int(y) - 1065353216)>>1) + 1065353216;

    return bit_cast_to_float(static_cast<int32_t>(i));
}

// Calculates approximate 1/square_root(x)
// it is faster than fast_invSqrt BUT
// use it in code not requiring precision
inline float approx_invSqrt(const float y)
{
    int i = 0x5f3759df - (bit_cast_to_int(y) >> 1);

    return bit_cast_to_float(static_cast<int32_t>(i));
}

// This function is a classic 1/square_root(x)code
// used by quake's game engine.
// It is very fast and has enough precision
// to drive a physics engine.
inline float fast_invSqrt(const float v)
{
    int i = 0x5f3759df - (bit_cast_to_int(v) >>1);
    float y = bit_cast_to_float(static_cast<int32_t>(i));

    y *= (1.5f - (0.5f * v * y * y));
    return y;
}

// It calculates a fast and accurate square_root(x)
inline float fast_sqrt(const float x)
{
    return x * fast_invSqrt(x);
}

inline float sign(const float x)
{
    return (x > 0.0f) ? 1.0f : (x < 0.0f) ? -1.0f : 0.0f;
}

// Ogre3 specific helpers
inline Ogre::Vector3 approx_normalise(Ogre::Vector3 v)
{
    return v*approx_invSqrt(v.squaredLength());
}

inline Ogre::Vector3 fast_normalise(Ogre::Vector3 v)
{
    return v*fast_invSqrt(v.squaredLength());
}

inline float approx_length(Ogre::Vector3 v)
{
    return approx_sqrt(v.squaredLength());
}

inline float fast_length(Ogre::Vector3 v)
{
    return fast_sqrt(v.squaredLength());
}

