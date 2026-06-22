#include "ParameterTreeBuilder.h"

#include "BooleanParameter.h"
#include "ChoiceParameter.h"
#include "Double2DParameter.h"
#include "DoubleParameter.h"
#include "IntParameter.h"
#include "ParameterGroup.h"
#include "PushButtonParameter.h"
#include "RGBAParameter.h"
#include "RootParameterGroup.h"
#include "StringParameter.h"

namespace MugLab::OfxBase {

// Maps each leaf Spec type to its ParameterNode concrete class via template specialization.
template <typename SpecType>
struct LeafParamTraits;

template <>
struct LeafParamTraits<BooleanParameter::Spec> {
    using type = BooleanParameter;
};
template <>
struct LeafParamTraits<ChoiceParameter::Spec> {
    using type = ChoiceParameter;
};
template <>
struct LeafParamTraits<Double2DParameter::Spec> {
    using type = Double2DParameter;
};
template <>
struct LeafParamTraits<DoubleParameter::Spec> {
    using type = DoubleParameter;
};
template <>
struct LeafParamTraits<IntParameter::Spec> {
    using type = IntParameter;
};
template <>
struct LeafParamTraits<PushButtonParameter::Spec> {
    using type = PushButtonParameter;
};
template <>
struct LeafParamTraits<RGBAParameter::Spec> {
    using type = RGBAParameter;
};
template <>
struct LeafParamTraits<StringParameter::Spec> {
    using type = StringParameter;
};

template <typename SpecType>
using LeafParamType = typename LeafParamTraits<SpecType>::type;

// Concept satisfied by all leaf Spec types (everything except GroupDecl).
template <typename SpecType>
concept LeafSpec = requires { typename LeafParamTraits<SpecType>::type; };

// --- define-time ---

static void defineNode(OFX::ImageEffectDescriptor& descriptor, const ParameterDecl& paramDecl,
                       OFX::GroupParamDescriptor* parentGroup);

static void defineGroupDecl(OFX::ImageEffectDescriptor& descriptor, const GroupDecl& groupDecl,
                            OFX::GroupParamDescriptor* parentGroup) {
    auto* groupDescriptor = descriptor.defineGroupParam(groupDecl.id_);
    if (!groupDecl.label_.empty() || !groupDecl.shortLabel_.empty() || !groupDecl.longLabel_.empty()) {
        const auto& shortLabel = groupDecl.shortLabel_.empty() ? groupDecl.label_ : groupDecl.shortLabel_;
        const auto& longLabel = groupDecl.longLabel_.empty() ? groupDecl.label_ : groupDecl.longLabel_;
        groupDescriptor->setLabels(groupDecl.label_, shortLabel, longLabel);
    }
    if (!groupDecl.hint_.empty()) {
        groupDescriptor->setHint(groupDecl.hint_);
    }
    groupDescriptor->setOpen(groupDecl.open_);
    if (parentGroup != nullptr) {
        groupDescriptor->setParent(*parentGroup);
    }
    for (const auto& childDecl : groupDecl.children_) {
        defineNode(descriptor, *childDecl, groupDescriptor);
    }
}

static void defineNode(OFX::ImageEffectDescriptor& descriptor, const ParameterDecl& paramDecl,
                       OFX::GroupParamDescriptor* parentGroup) {
    std::visit(
        [&](const auto& spec) -> void {
            using SpecType = std::decay_t<decltype(spec)>;
            if constexpr (std::is_same_v<SpecType, GroupDecl>) {
                defineGroupDecl(descriptor, spec, parentGroup);
            } else {
                static_assert(LeafSpec<SpecType>, "Unhandled ParameterDecl leaf type");
                LeafParamType<SpecType> tempParam{spec};
                tempParam.define(descriptor, parentGroup);
            }
        },
        paramDecl);
}

void defineTree(OFX::ImageEffectDescriptor& descriptor, const GroupDecl& groupDecl,
                OFX::GroupParamDescriptor* parentGroup) {
    defineGroupDecl(descriptor, groupDecl, parentGroup);
}

// --- instance-time ---

// Forward declaration for mutual recursion between buildGroup and addFetchedChild.
template <typename GroupType>
static auto buildGroup(OFX::ImageEffect& effectInstance, const GroupDecl& groupDecl,
                       GroupType& group) -> std::unique_ptr<GroupParameter>;

template <typename GroupType>
static void addFetchedChild(OFX::ImageEffect& effectInstance, const ParameterDecl& paramDecl,
                            GroupParameter& parentGroup, GroupType& group) {
    std::visit(
        [&](const auto& spec) -> void {
            using SpecType = std::decay_t<decltype(spec)>;
            if constexpr (std::is_same_v<SpecType, GroupDecl>) {
                auto childGroup = buildGroup(effectInstance, spec, group);
                parentGroup.addOwned(std::move(childGroup));
            } else {
                static_assert(LeafSpec<SpecType>, "Unhandled ParameterDecl leaf type");
                auto childParam = std::make_unique<LeafParamType<SpecType>>(spec);
                childParam->fetch(effectInstance);
                if (spec.visibility_) {
                    group.registerVisibilityRule(spec.id_, *spec.visibility_);
                }
                parentGroup.addOwned(std::move(childParam));
            }
        },
        paramDecl);
}

template <typename GroupType>
static auto buildGroup(OFX::ImageEffect& effectInstance, const GroupDecl& groupDecl,
                       GroupType& group) -> std::unique_ptr<GroupParameter> {
    auto rootGroup = GroupParameter::create(GroupParameter::Spec{.id_ = groupDecl.id_,
                                                                 .slot_ = groupDecl.slot_,
                                                                 .label_ = groupDecl.label_,
                                                                 .shortLabel_ = groupDecl.shortLabel_,
                                                                 .longLabel_ = groupDecl.longLabel_,
                                                                 .hint_ = groupDecl.hint_,
                                                                 .open_ = groupDecl.open_});
    rootGroup->fetch(effectInstance);
    for (const auto& childDecl : groupDecl.children_) {
        addFetchedChild(effectInstance, *childDecl, *rootGroup, group);
    }
    return rootGroup;
}

template <typename GroupType>
static void registerVisibilityRules(const GroupDecl& groupDecl, GroupType& group) {
    if (groupDecl.visibility_) {
        group.registerVisibilityRule(groupDecl.id_, *groupDecl.visibility_);
    }
    for (const auto& childDecl : groupDecl.children_) {
        std::visit(
            [&](const auto& spec) -> void {
                if constexpr (std::is_same_v<std::decay_t<decltype(spec)>, GroupDecl>) {
                    registerVisibilityRules(spec, group);
                }
            },
            *childDecl);
    }
}

void fetchTree(OFX::ImageEffect& effectInstance, const GroupDecl& groupDecl, ParameterGroup& group) {
    group.setTree(buildGroup(effectInstance, groupDecl, group));
    registerVisibilityRules(groupDecl, group);
}

void defineTree(OFX::ImageEffectDescriptor& descriptor, const RootDecl& decl) {
    for (const auto& childDecl : decl.children_) {
        defineNode(descriptor, *childDecl, nullptr);
    }
}

static void addFetchedChildToRoot(OFX::ImageEffect& effectInstance, const ParameterDecl& paramDecl,
                                  RootParameterGroup& group) {
    std::visit(
        [&](const auto& spec) -> void {
            using SpecType = std::decay_t<decltype(spec)>;
            if constexpr (std::is_same_v<SpecType, GroupDecl>) {
                auto child = buildGroup(effectInstance, spec, group);
                registerVisibilityRules(spec, group);
                group.addChild(std::move(child));
            } else {
                static_assert(LeafSpec<SpecType>, "Unhandled ParameterDecl leaf type");
                auto child = std::make_unique<LeafParamType<SpecType>>(spec);
                child->fetch(effectInstance);
                if (spec.visibility_) {
                    group.registerVisibilityRule(spec.id_, *spec.visibility_);
                }
                group.addChild(std::move(child));
            }
        },
        paramDecl);
}

void fetchTree(OFX::ImageEffect& effectInstance, const RootDecl& decl, RootParameterGroup& group) {
    for (const auto& childDecl : decl.children_) {
        addFetchedChildToRoot(effectInstance, *childDecl, group);
    }
}

}  // namespace MugLab::OfxBase
