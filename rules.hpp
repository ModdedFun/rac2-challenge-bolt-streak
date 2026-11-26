// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace bolt_streak {

enum class Damage { Default, Lenient, OnDeath };
enum class Curve { Default, Linear, Proposed };

struct Rules {
    int cap;
    int credit_per_kill;
};

constexpr Rules rules(Curve curve, int cycle) {
    if (cycle <= 1 || curve == Curve::Default) return {20, 4};
    if (curve == Curve::Linear) return {20 * cycle, 4 * cycle};
    constexpr Rules proposed[] = {{20, 4}, {50, 7}, {100, 10}, {200, 16},
                                  {300, 24}, {400, 32}, {500, 40}};
    if (cycle <= 7) return proposed[cycle - 1];
    return {500 + 100 * (cycle - 7), 40 + 8 * (cycle - 7)};
}

struct Streak {
    int multiplier = 1;
    int credit = 0; // Quarter-kills. The game's progress byte counts whole kills.

    int native_multiplier() const { return std::min(multiplier, 20); }
    int native_progress() const { return multiplier >= 20 ? 0 : credit / 4; }

    void kill(Rules rule) {
        if (multiplier >= rule.cap) return;
        credit += rule.credit_per_kill;
        while (multiplier < rule.cap) {
            const int cost = 4 * std::min(10, (multiplier + 1) / 2);
            if (credit < cost) return;
            credit -= cost;
            ++multiplier;
        }
        credit = 0;
    }

    void hit(Damage damage) {
        if (damage == Damage::OnDeath) return;
        multiplier = damage == Damage::Lenient ? std::max(1, multiplier - 1) : 1;
        credit = 0;
    }
};

} // namespace bolt_streak
