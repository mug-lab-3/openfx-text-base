#pragma once

#include <blend2d.h>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "interaction/SelectionItem.h"

namespace MugLab::OfxBase {

using IntentValue = std::variant<BLBox, BLPoint, double, bool, SelectionItem>;
using IntentValues = std::vector<IntentValue>;
using IntentMap = std::unordered_map<std::string, std::optional<IntentValues>>;

class IntentRegistry {
   public:
    IntentRegistry();
    ~IntentRegistry();

    void emit(const std::string& useCaseName, const std::string& key, const IntentValues& values);
    void clear(const std::string& useCaseName);
    void clear(const std::string& useCaseName, const std::string& key);

    [[nodiscard]] auto has(const std::string& key) const -> bool;
    [[nodiscard]] auto getGroups(const std::string& key) const -> std::vector<IntentValues>;
    [[nodiscard]] auto getFlattened(const std::string& key) const -> IntentValues;

    [[nodiscard]] auto getRegistry() const
        -> const std::unordered_map<std::string, std::unordered_map<std::string, IntentValues>>&;

    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto size() const -> std::size_t;

   private:
    std::unordered_map<std::string, std::unordered_map<std::string, IntentValues>> registry_;
};

auto isInMarqueeSelection(const IntentRegistry& intents, const SelectionItem& target) -> bool;

}  // namespace MugLab::OfxBase
