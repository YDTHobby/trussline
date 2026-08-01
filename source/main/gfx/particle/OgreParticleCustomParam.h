#pragma once
#ifndef __OGRE_PARTICLE_CUSTOM_PARAM_H__
#define __OGRE_PARTICLE_CUSTOM_PARAM_H__

#include <OgreParticle.h>

namespace Ogre {

/// @addtogroup Gfx
/// @{

/// @addtogroup Particle
/// @{

/// Custom visual data for the shader renderer.
///
/// VESTIGIAL as of the OGRE 14 upgrade. It used to derive from
/// ParticleVisualData and was handed out per-particle by
/// ShaderParticleRenderer::_createVisualData(). OGRE 14 removed that mechanism
/// entirely - ParticleVisualData is now only a forward declaration and
/// Particle::getVisualData() is gone - so nothing constructs or reads this any
/// more, and it can no longer inherit from a class that has no definition.
///
/// Kept (standalone) rather than deleted because the shader-side custom param
/// is a real feature that Phase 6 may want to reinstate through some other
/// mechanism. If Phase 6 decides against it, delete this header and its
/// CMakeLists entry.
class ParticleCustomParam
{
public:
    ParticleCustomParam() : paramValue(0, 0, 0, 0)
    {
    }

    virtual ~ParticleCustomParam()
    {
    }

    Vector4 paramValue;
};

/// @} // addtogroup Particle
/// @} // addtogroup Gfx

} // namespace Ogre

#endif // __OGRE_PARTICLE_CUSTOM_PARAM_H__
