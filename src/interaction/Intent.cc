#include "interaction/Intent.h"

#include <algorithm>

namespace MugLab::OfxBase {

IntentRegistry::IntentRegistry() = default;

IntentRegistry::~IntentRegistry() = default;

void IntentRegistry::emit(const std::string& useCaseName, const std::string& key, const IntentValues& values) {
    registry_[key][useCaseName] = values;
}

void IntentRegistry::clear(const std::string& useCaseName) {
    for (auto& [key, owners] : registry_) {
        owners.erase(useCaseName);
    }
}

void IntentRegistry::clear(const std::string& useCaseName, const std::string& key) {
    const auto iterator = registry_.find(key);
    if (iterator != registry_.end()) {
        iterator->second.erase(useCaseName);
    }
}

auto IntentRegistry::has(const std::string& key) const -> bool {
    const auto iterator = registry_.find(key);
    bool hasActiveIntent = false;

    if (iterator != registry_.end()) {
        for (const auto& [owner, values] : iterator->second) {
            if (!values.empty()) {
                hasActiveIntent = true;
                break;
            }
        }
    }

    return hasActiveIntent;
}

auto IntentRegistry::getGroups(const std::string& key) const -> std::vector<IntentValues> {
    const auto iterator = registry_.find(key);
    std::vector<IntentValues> groups;

    if (iterator != registry_.end()) {
        for (const auto& [owner, values] : iterator->second) {
            if (!values.empty()) {
                groups.push_back(values);
            }
        }
    }

    return groups;
}

auto IntentRegistry::getFlattened(const std::string& key) const -> IntentValues {
    const auto iterator = registry_.find(key);
    IntentValues flattenedValues;

    if (iterator != registry_.end()) {
        for (const auto& [owner, values] : iterator->second) {
            flattenedValues.insert(flattenedValues.end(), values.begin(), values.end());
        }
    }

    return flattenedValues;
}

auto IntentRegistry::getRegistry() const
    -> const std::unordered_map<std::string, std::unordered_map<std::string, IntentValues>>& {
    return registry_;
}

auto IntentRegistry::empty() const -> bool {
    bool isEmpty = true;

    for (const auto& [key, owners] : registry_) {
        for (const auto& [owner, values] : owners) {
            if (!values.empty()) {
                isEmpty = false;
                break;
            }
        }
        if (!isEmpty) {
            break;
        }
    }

    return isEmpty;
}

auto IntentRegistry::size() const -> std::size_t {
    std::size_t count = 0;

    for (const auto& [key, owners] : registry_) {
        for (const auto& [owner, values] : owners) {
            if (!values.empty()) {
                count++;
                break;
            }
        }
    }

    return count;
}

auto isInMarqueeSelection(const IntentRegistry& intents, const SelectionItem& target) -> bool {
    const std::vector<IntentValues> groups = intents.getGroups("marqueeSelection");
    bool isContained = false;

    for (const auto& group : groups) {
        const bool matched = std::ranges::any_of(group, [&](const IntentValue& value) -> bool {
            const auto* item = std::get_if<SelectionItem>(&value);
            bool isTargetMatch = false;
            if (item != nullptr) {
                isTargetMatch =
                    (item->characterIndex_ == target.characterIndex_ && item->partIndex_ == target.partIndex_);
            }
            return isTargetMatch;
        });

        if (matched) {
            isContained = true;
            break;
        }
    }

    return isContained;
}

}  // namespace MugLab::OfxBase
