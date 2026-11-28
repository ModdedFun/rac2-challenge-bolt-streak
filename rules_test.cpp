// SPDX-License-Identifier: GPL-3.0-or-later
#include "rules.hpp"

using namespace bolt_streak;

constexpr int caps[] = {20, 50, 100, 200, 300, 400, 500, 600};
constexpr int rates[] = {4, 7, 10, 16, 24, 32, 40, 48};

int main() {
    if (std::string_view(hud_format(20).data()) != "x%d" ||
        std::string_view(hud_format(21).data()) != "x21" ||
        std::string_view(hud_format(500).data()) != "x500" ||
        std::string_view(hud_format(rules(Curve::Proposed, 32767).cap).data()) != "3276500") return 8;
    for (int i = 0; i < 8; ++i) {
        const int cycle = i + 1;
        const auto proposed = rules(Curve::Proposed, cycle);
        const auto linear = rules(Curve::Linear, cycle);
        const auto stock = rules(Curve::Default, cycle);
        if (proposed.cap != caps[i] || proposed.credit_per_kill != rates[i]
            || linear.cap != 20 * cycle || linear.credit_per_kill != 4 * cycle
            || stock.cap != 20 || stock.credit_per_kill != 4) return 1;
        Streak streak;
        for (int kills = 0; kills < 10 * proposed.cap; ++kills) streak.kill(proposed);
        if (streak.multiplier != proposed.cap || streak.credit != 0
            || streak.native_multiplier() != 20 || streak.native_progress() != 0) return 2;
    }
    Streak streak;
    streak.kill(rules(Curve::Proposed, 2));
    if (streak.multiplier != 2 || streak.credit != 3) return 3;
    streak.kill(rules(Curve::Proposed, 2));
    if (streak.multiplier != 3 || streak.credit != 6) return 4;
    streak.hit(Damage::OnDeath);
    if (streak.multiplier != 3 || streak.credit != 6) return 5;
    streak.hit(Damage::Lenient);
    if (streak.multiplier != 2 || streak.credit != 0) return 6;
    streak.hit(Damage::Default);
    if (streak.multiplier != 1 || streak.credit != 0) return 7;
}
