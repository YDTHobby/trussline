// Trussline Spike A - does OGRE 14 + Vulkan actually render on Android?
//
// THROWAWAY (ROADMAP 1.1). The deliverable is the answer, not this code.
//
// Milestone ladder, deliberately in this order so a failure localises itself:
//   1. OGRE Root constructs, Vulkan plugin installs, renderer is listed
//   2. RenderWindow created from the ANativeWindow  <- surface + swapchain
//   3. Frames present, with a cycling clear colour  <- the loop actually runs
//   4. Geometry draws                                <- shaders / RTSS
//
// Everything is logged to logcat under "TrusslineSpikeA" because on a spike the
// log IS the result.

#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <Ogre.h>
#include <OgreVulkanPlugin.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "TrusslineSpikeA", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "TrusslineSpikeA", __VA_ARGS__)

namespace {

struct SpikeState
{
    Ogre::Root*          root     = nullptr;
    Ogre::VulkanPlugin*  vulkan   = nullptr;
    Ogre::RenderWindow*  window   = nullptr;
    Ogre::SceneManager*  scene    = nullptr;
    Ogre::Camera*        camera   = nullptr;
    Ogre::Viewport*      viewport = nullptr;
    bool                 ready    = false;
    unsigned long        frames   = 0;
};

SpikeState g_spike;

// App-private writable dir, captured at init so the render loop can write there.
char g_dataPath[512] = {0};

void InitOgre(android_app* app)
{
    if (g_spike.ready)
    {
        return;
    }

    try
    {
        std::snprintf(g_dataPath, sizeof(g_dataPath), "%s", app->activity->internalDataPath);

        const std::string logPath =
            std::string(app->activity->internalDataPath) + "/Ogre.log";

        LOGI("=== MILESTONE 1: constructing Ogre::Root, log -> %s ===", logPath.c_str());

        // No plugins.cfg and no resources.cfg: this is a static build, so the
        // Vulkan plugin is linked in and installed by hand below.
        g_spike.root = new Ogre::Root("", "", logPath);

        g_spike.vulkan = new Ogre::VulkanPlugin();
        g_spike.root->installPlugin(g_spike.vulkan);

        const Ogre::RenderSystemList& renderers = g_spike.root->getAvailableRenderers();
        LOGI("available renderers: %zu", renderers.size());
        for (Ogre::RenderSystem* rs : renderers)
        {
            LOGI("  - %s", rs->getName().c_str());
        }

        if (renderers.empty())
        {
            LOGE("FAIL: no render systems registered - Vulkan plugin did not install");
            return;
        }

        g_spike.root->setRenderSystem(renderers[0]);
        g_spike.root->initialise(false);
        LOGI("MILESTONE 1 OK: render system '%s' initialised", renderers[0]->getName().c_str());

        const int width  = ANativeWindow_getWidth(app->window);
        const int height = ANativeWindow_getHeight(app->window);
        LOGI("=== MILESTONE 2: creating RenderWindow %dx%d from ANativeWindow %p ===",
             width, height, (void*)app->window);

        Ogre::NameValuePairList params;
        params["externalWindowHandle"] =
            Ogre::StringConverter::toString(reinterpret_cast<size_t>(app->window));

        g_spike.window = g_spike.root->createRenderWindow("SpikeA", width, height, false, &params);

        if (g_spike.window == nullptr)
        {
            LOGE("FAIL: createRenderWindow returned null");
            return;
        }
        LOGI("MILESTONE 2 OK: RenderWindow created (%ux%u)",
             g_spike.window->getWidth(), g_spike.window->getHeight());

        g_spike.scene    = g_spike.root->createSceneManager();
        g_spike.camera   = g_spike.scene->createCamera("SpikeCam");
        g_spike.viewport = g_spike.window->addViewport(g_spike.camera);
        g_spike.viewport->setBackgroundColour(Ogre::ColourValue(0.10f, 0.20f, 0.40f));

        g_spike.ready = true;
        LOGI("=== INIT COMPLETE - entering render loop ===");
    }
    catch (const Ogre::Exception& e)
    {
        LOGE("FAIL: Ogre::Exception during init: %s", e.getFullDescription().c_str());
    }
    catch (const std::exception& e)
    {
        LOGE("FAIL: std::exception during init: %s", e.what());
    }
}

void ShutdownOgre()
{
    LOGI("shutting down after %lu frames", g_spike.frames);

    // Order matters: Root owns the render window and scene manager.
    delete g_spike.root;
    g_spike.root     = nullptr;
    g_spike.window   = nullptr;
    g_spike.scene    = nullptr;
    g_spike.camera   = nullptr;
    g_spike.viewport = nullptr;

    // The plugin was installed into Root but is owned here.
    delete g_spike.vulkan;
    g_spike.vulkan = nullptr;

    g_spike.ready  = false;
    g_spike.frames = 0;
}

void RenderFrame()
{
    if (!g_spike.ready)
    {
        return;
    }

    try
    {
        // Cycle the clear colour so a static screenshot cannot be mistaken for a
        // presenting swapchain - if the colour moves, frames are really landing.
        const float t = static_cast<float>(g_spike.frames) * 0.01f;
        g_spike.viewport->setBackgroundColour(Ogre::ColourValue(
            0.5f + 0.5f * std::sin(t),
            0.5f + 0.5f * std::sin(t + 2.09f),
            0.5f + 0.5f * std::sin(t + 4.19f)));

        g_spike.root->renderOneFrame();
        ++g_spike.frames;

        if (g_spike.frames == 1)
        {
            LOGI("=== MILESTONE 3: first frame presented ===");
        }
        else if (g_spike.frames == 300)
        {
            // Ask OGRE what it actually rendered.
            //
            // `adb screencap` is useless here: it returns a static black image
            // whether the app draws black or the capture path simply cannot see a
            // GPU-composited Vulkan surface, so it discriminates nothing. A
            // render-target readback comes from inside the renderer and does.
            // Read the framebuffer straight into memory. Every file-based route
            // was a dead end - .png has no codec linked, .dds rejects non-power-
            // of-two (the window is 2400x1080), .ktx cannot encode at all - and
            // none of those were rendering failures, just codec gaps. Reading
            // pixels needs no codec and answers the actual question.
            // CONTROL TEST: clear an offscreen render target to pure red and read
            // it back. This decides between two very different conclusions:
            //   red   -> OGRE's clear path works; the swapchain readback simply
            //            does not sample the image that was presented
            //   black -> the clear is genuinely not happening anywhere
            try
            {
                Ogre::TexturePtr rtt = Ogre::TextureManager::getSingleton().createManual(
                    "SpikeRTT", Ogre::RGN_DEFAULT, Ogre::TEX_TYPE_2D,
                    256, 256, 0, Ogre::PF_BYTE_RGBA, Ogre::TU_RENDERTARGET);

                Ogre::RenderTexture* rt = rtt->getBuffer()->getRenderTarget();
                Ogre::Viewport* rvp = rt->addViewport(g_spike.camera);
                rvp->setBackgroundColour(Ogre::ColourValue(1.0f, 0.0f, 0.0f, 1.0f));
                rvp->setClearEveryFrame(true);
                rt->update();

                Ogre::uchar rbuf[4 * 4 * 4];
                std::memset(rbuf, 0xAB, sizeof(rbuf));
                Ogre::PixelBox rpb(4, 4, 1, Ogre::PF_BYTE_RGBA, rbuf);
                rt->copyContentsToMemory(Ogre::Box(128, 128, 132, 132), rpb);

                LOGI("=== CONTROL: offscreen RTT centre RGBA = %u %u %u %u (expected 255 0 0 255) ===",
                     rbuf[0], rbuf[1], rbuf[2], rbuf[3]);
            }
            catch (const Ogre::Exception& e)
            {
                LOGE("offscreen control test failed: %s", e.getDescription().c_str());
            }

            try
            {
                // Sentinel-fill, NOT zero-fill. Zeroing makes "the frame is
                // black" and "copyContentsToMemory silently wrote nothing"
                // indistinguishable - and Vulkan swapchain readback is very
                // often unimplemented. If these bytes survive the call, the
                // readback is a no-op and tells us nothing about the frame.
                Ogre::uchar buf[4 * 4 * 4];
                std::memset(buf, 0xAB, sizeof(buf));
                Ogre::PixelBox pb(4, 4, 1, Ogre::PF_BYTE_RGBA, buf);

                const Ogre::uint32 cx = g_spike.window->getWidth() / 2;
                const Ogre::uint32 cy = g_spike.window->getHeight() / 2;
                g_spike.window->copyContentsToMemory(Ogre::Box(cx, cy, cx + 4, cy + 4), pb);

                // What the clear colour was for the frame just presented.
                const float t = static_cast<float>(g_spike.frames - 1) * 0.01f;
                const Ogre::ColourValue expected(0.5f + 0.5f * std::sin(t),
                                                 0.5f + 0.5f * std::sin(t + 2.09f),
                                                 0.5f + 0.5f * std::sin(t + 4.19f));

                LOGI("=== MILESTONE 3b: centre pixel RGBA = %u %u %u %u ===",
                     buf[0], buf[1], buf[2], buf[3]);
                LOGI("    expected approx  RGBA = %u %u %u 255",
                     (unsigned)(expected.r * 255.0f),
                     (unsigned)(expected.g * 255.0f),
                     (unsigned)(expected.b * 255.0f));

                bool untouched = true;
                for (size_t i = 0; i < sizeof(buf); ++i)
                {
                    if (buf[i] != 0xAB) { untouched = false; break; }
                }

                if (untouched)
                {
                    LOGE("    VERDICT: INCONCLUSIVE - sentinel 0xAB survived, "
                         "copyContentsToMemory wrote nothing (readback unimplemented)");
                }
                else if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0)
                {
                    LOGE("    VERDICT: BLACK - readback worked and the frame really is black");
                }
                else
                {
                    LOGI("    VERDICT: NON-BLACK - clear colour is in the framebuffer");
                }
            }
            catch (const Ogre::Exception& e)
            {
                LOGE("pixel readback unsupported: %s", e.getDescription().c_str());
            }
        }
        else if (g_spike.frames % 120 == 0)
        {
            LOGI("frames presented: %lu", g_spike.frames);
        }
    }
    catch (const Ogre::Exception& e)
    {
        LOGE("FAIL: Ogre::Exception in render loop: %s", e.getFullDescription().c_str());
        g_spike.ready = false;
    }
}

void HandleCmd(android_app* app, int32_t cmd)
{
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
        LOGI("APP_CMD_INIT_WINDOW");
        if (app->window != nullptr)
        {
            InitOgre(app);
        }
        break;

    case APP_CMD_TERM_WINDOW:
        LOGI("APP_CMD_TERM_WINDOW");
        ShutdownOgre();
        break;

    case APP_CMD_DESTROY:
        LOGI("APP_CMD_DESTROY");
        ShutdownOgre();
        break;

    default:
        break;
    }
}

} // namespace

extern "C" void android_main(android_app* app)
{
    LOGI("android_main entered");
    app->onAppCmd = HandleCmd;

    while (true)
    {
        int events = 0;
        android_poll_source* source = nullptr;

        // The timeout MUST be re-evaluated on every iteration, which is why the
        // expression lives inside the call rather than in a variable hoisted
        // above the loop. Hoisting it deadlocks: we enter with ready==false and
        // therefore timeout==-1 (block forever), APP_CMD_INIT_WINDOW arrives and
        // sets ready==true, but the already-captured -1 makes the next poll block
        // for an event that never comes on an idle app - so the render loop is
        // never reached and not one frame is ever drawn.
        while (ALooper_pollOnce(g_spike.ready ? 0 : -1, nullptr, &events, (void**)&source) >= 0)
        {
            if (source != nullptr)
            {
                source->process(app, source);
            }
            if (app->destroyRequested != 0)
            {
                LOGI("destroyRequested - exiting android_main");
                ShutdownOgre();
                return;
            }
        }

        RenderFrame();
    }
}
