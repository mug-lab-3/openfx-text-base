#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#define GetCurrentProcessId getpid
#endif

#include <blend2d.h>

#include "ofxDrawSuite.h"
#include "ofxsImageEffect.h"
#include "ofxsInteract.h"

#include "debugger/LogManager.h"
#include "font/FontManager.h"
#include "main.h"
#include "render/Renderer.h"
#include "Version.h"
#include "interaction/Interact.h"

// ==============================================================================
// OfxBasePlugin
// ==============================================================================
OfxBasePlugin::OfxBasePlugin(OfxImageEffectHandle handle)
    : OFX::ImageEffect(handle), params_(*this) {
    LOG_INFO("LIFECYCLE_INSTANCE_CREATE", "handle", reinterpret_cast<uint64_t>(handle));
    dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
    srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
    params_.updateVisibility(0.0);
}

OfxBasePlugin::~OfxBasePlugin() {
    LOG_INFO("LIFECYCLE_INSTANCE_DESTROY");
}

void OfxBasePlugin::changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName) {
    // Skip eChangeTime to avoid redundant UI refreshes while the timeline is playing.
    // Visibility only needs updating when the user actually edits a parameter.
    if (args.reason == OFX::eChangeTime) { return; }
    LOG_INFO("EVENT_PARAM_CHANGED", "param", paramName, "reason", static_cast<int>(args.reason));
    params_.updateVisibility(args.time);
    if (commandRouter_.isCommandTrigger(paramName)) {
        commandRouter_.onTriggerCommand(paramName, args.time, params_);
    }
}

void OfxBasePlugin::render(const OFX::RenderArguments& args) {
    static thread_local MugLab::OfxBase::Renderer renderer;
    try {
        renderer.render(*this, args);
    } catch (const std::bad_alloc& e) {
        LOG_ERROR("RENDER_BAD_ALLOC", "message", e.what());
    } catch (const std::exception& e) {
        LOG_ERROR("RENDER_EXCEPTION", "message", e.what());
    } catch (...) {
        LOG_ERROR("RENDER_EXCEPTION", "message", "Unknown exception");
    }
}

// ==============================================================================
// OfxBaseOverlayDescriptor: wires up Interact via V2 entry point
// ==============================================================================
class OfxBaseOverlayDescriptor : public OFX::EffectOverlayDescriptor,
                              public OFX::InteractMainEntry<OfxBaseOverlayDescriptor> {
   public:
    using InteractMainEntry<OfxBaseOverlayDescriptor>::overlayInteractMainEntry;

    auto createInstance(OfxInteractHandle handle, OFX::ImageEffect* effect) -> OFX::OverlayInteract* override {
        auto* plugin = static_cast<OfxBasePlugin*>(effect);
        return new MugLab::OfxBase::Interact(handle, effect, plugin->params_);
    }

    OfxPluginEntryPoint* getMainEntry() override {
        return overlayInteractMainEntry;
    }
};

// Injects DrawContext into Interact before draw() is called (V2 protocol).
static auto ofxBaseInteractEntryV2(const char* action, const void* handle,
                                OfxPropertySetHandle inArgs,
                                OfxPropertySetHandle outArgs) -> OfxStatus {
    static auto* interactSuite = const_cast<OfxInteractSuiteV1*>(static_cast<const OfxInteractSuiteV1*>(OFX::fetchSuite(kOfxInteractSuite, 1, true)));
    static auto* propSuite     = const_cast<OfxPropertySuiteV1*>(static_cast<const OfxPropertySuiteV1*>(OFX::fetchSuite(kOfxPropertySuite, 1, true)));

    if ((interactSuite == nullptr) || (propSuite == nullptr)) {
        return kOfxStatErrMissingHostFeature;
    }

    MugLab::OfxBase::Interact* interact = nullptr;
    if (handle != nullptr) {
        OfxPropertySetHandle interactPropSet = nullptr;
        interactSuite->interactGetPropertySet(static_cast<OfxInteractHandle>(const_cast<void*>(handle)), &interactPropSet);
        if (interactPropSet != nullptr) {
            void* data = nullptr;
            propSuite->propGetPointer(interactPropSet, kOfxPropInstanceData, 0, &data);
            interact = static_cast<MugLab::OfxBase::Interact*>(data);
        }
    }

    const bool isDraw = (action != nullptr) && (std::strcmp(action, kOfxInteractActionDraw) == 0);
    if (isDraw && (inArgs != nullptr) && (interact != nullptr)) {
        void* ctx = nullptr;
        if (propSuite->propGetPointer(inArgs, kOfxInteractPropDrawContext, 0, &ctx) == kOfxStatOK) {
            interact->currentContext_ = static_cast<OfxDrawContextHandle>(ctx);
        }
    }

    OfxStatus stat = OfxBaseOverlayDescriptor::overlayInteractMainEntry(action, handle, inArgs, outArgs);

    if (isDraw && (interact != nullptr)) {
        interact->currentContext_ = nullptr;
    }
    return stat;
}

// Wraps the Support Library interact entry for V1 (legacy OpenGL fallback).
// Note: Commented out as OpenGL fallback is no longer used (OFX v1.5 is the minimum).
// static OfxStatus ofxBaseInteractEntryV1(const char* action, const void* handle, OfxPropertySetHandle inArgs,
//                                         OfxPropertySetHandle outArgs) {
//     OfxStatus status = OfxBaseOverlayDescriptor::overlayInteractMainEntry(action, handle, inArgs, outArgs);
//     return status;
// }

// ==============================================================================
// OfxBasePluginFactory
// ==============================================================================
class OfxBasePluginFactory : public OFX::PluginFactoryHelper<OfxBasePluginFactory> {
   public:
    OfxBasePluginFactory(const std::string& id, unsigned int verMaj, unsigned int verMin)
        : OFX::PluginFactoryHelper<OfxBasePluginFactory>(id, verMaj, verMin) {}

    void load() override {
        LOG_INFO("LIFECYCLE_LOAD", "plugin_id", "com.muglab.ofxbase");
        MugLab::OfxBase::FontManager::update();
    }

    void unload() override {
        LOG_INFO("LIFECYCLE_UNLOAD", "plugin_id", "com.muglab.ofxbase");
        MugLab::OfxBase::FontManager::cleanup();
    }

    void describe(OFX::ImageEffectDescriptor& desc) override {
        desc.setLabels("OfxBase Plugin", "OfxBase Plugin", "OfxBase Plugin");
        desc.setPluginGrouping("OfxBase");
        desc.addSupportedContext(OFX::eContextFilter);
        desc.addSupportedContext(OFX::eContextGenerator);
        desc.addSupportedBitDepth(OFX::eBitDepthFloat);
        desc.addSupportedBitDepth(OFX::eBitDepthUShort);
        desc.addSupportedBitDepth(OFX::eBitDepthUByte);
        desc.setSupportsTiles(false);
        desc.setSupportsMultiResolution(true);
        desc.setRenderThreadSafety(OFX::eRenderInstanceSafe);

        auto* interactDesc = new OfxBaseOverlayDescriptor();
        desc.setOverlayInteractDescriptor(interactDesc);

        // Register V2 entry point (OfxDrawSuiteV1). If the host does not support V2,
        // propSetPointer returns an error which is silently ignored here; draw() guards
        // against a null DrawContext so the overlay simply does not render on legacy hosts.
        desc.getPropertySet().propSetPointer(kOfxImageEffectPluginPropOverlayInteractV2,
                                             reinterpret_cast<void*>(ofxBaseInteractEntryV2));
        // desc.getPropertySet().propSetPointer(kOfxImageEffectPluginPropOverlayInteractV1,
        //                                      (void*)ofxBaseInteractEntryV1);

#ifdef __APPLE__
        desc.setSupportsMetalRender(true);
#else
        // To enable CUDA GPU rendering, uncomment the lines below and implement
        // the CUDA kernel and compositor in the render() method.
        // desc.setSupportsCudaRender(true);
        // desc.setSupportsCudaStream(true);
        desc.setSupportsOpenCLBuffersRender(true);
#endif
    }

    void describeInContext(OFX::ImageEffectDescriptor& desc, OFX::ContextEnum /*context*/) override {
        auto* src = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
        src->addSupportedComponent(OFX::ePixelComponentRGBA);
        src->setOptional(true);

        desc.defineClip(kOfxImageEffectOutputClipName)->addSupportedComponent(OFX::ePixelComponentRGBA);

        MugLab::OfxBase::ParameterManager::defineAllParameters(desc);
    }

    auto createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/) -> OFX::ImageEffect* override {
        return new OfxBasePlugin(handle);
    }
};

namespace OFX::Plugin {
void getPluginIDs(OFX::PluginFactoryArray& ids) {
    static OfxBasePluginFactory factory("com.muglab.ofxbase",
                                        MugLab::OfxBase::kVersionMajor,
                                        MugLab::OfxBase::kVersionMinor);
    ids.push_back(&factory);
}
}  // namespace OFX::Plugin
