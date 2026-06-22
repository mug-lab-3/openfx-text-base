#pragma once

#include <memory>

#include "GroupParameter.h"
#include "ParameterDecl.h"
#include "ParameterGroup.h"
#include "RootParameterGroup.h"
#include "ofxsImageEffect.h"

namespace MugLab::OfxBase {

// Walks a GroupDecl tree and calls descriptor.defineXxxParam for every node.
// Called once at describe-time (no ParameterManager instance exists yet).
void defineTree(OFX::ImageEffectDescriptor& descriptor, const GroupDecl& decl,
                OFX::GroupParamDescriptor* parentGroup = nullptr);

// Walks a RootDecl and calls descriptor.defineXxxParam for every child node
// without creating an OFX GroupParam for the root itself.
void defineTree(OFX::ImageEffectDescriptor& descriptor, const RootDecl& decl);

// Walks a GroupDecl tree, instantiates XxxParameter objects, calls fetch() on
// each, registers them in the store, and returns the root GroupParameter.
// Called at instance-time inside ParameterManager's constructor.
void fetchTree(OFX::ImageEffect& effectInstance, const GroupDecl& decl, ParameterGroup& group);

// Walks a RootDecl and fetches each child directly into a RootParameterGroup.
void fetchTree(OFX::ImageEffect& effectInstance, const RootDecl& decl, RootParameterGroup& group);

}  // namespace MugLab::OfxBase
