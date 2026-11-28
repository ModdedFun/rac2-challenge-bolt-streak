// SPDX-License-Identifier: GPL-3.0-or-later
#include "openrac/games/rac2.hpp"
#include "openrac/mod.hpp"
#include "rules.hpp"

#include <array>
#include <memory>

namespace {

using bolt_streak::Curve;
using bolt_streak::Damage;
using openrac::gaddr;

constexpr gaddr kCycle = 0x12u;
constexpr gaddr kReserve = 0x1Cu;
constexpr gaddr kStreak = 0x3Au;
constexpr gaddr kProgress = 0x3Bu;
constexpr gaddr kHealth = 0x24CCu;

struct Disc {
    const char* version;
    gaddr save;
    gaddr hero;
    gaddr reward_slot;
    gaddr bestiary;
    gaddr hud_format;
};

constexpr Disc kDiscs[] = {
    {"pal",   0x001A7A80u, 0x00189EA0u, 0x001A9EF0u, 0x002643B0u, 0x001AE778u},
    {"ntsc",  0x001A79F8u, 0x00189E20u, 0x001A9E70u, 0x00264630u, 0x001AE708u},
    {"ntsc2", 0x001A7A00u, 0x00189E20u, 0x001A9E70u, 0x002645F0u, 0x001AE6C0u},
};

struct Drop {
    gaddr moby = 0;
    int native_streak = 1;
    int streak = 1;
    int reserve = 0;
};

Damage damage_setting(openrac::Mod& mod) {
    const auto value = mod.setting<std::string>("damage_policy");
    if (value == "lenient") return Damage::Lenient;
    if (value == "on_death") return Damage::OnDeath;
    return Damage::Default;
}

Curve curve_setting(openrac::Mod& mod) {
    const auto value = mod.setting<std::string>("growth_curve");
    if (value == "linear") return Curve::Linear;
    if (value == "proposed") return Curve::Proposed;
    return Curve::Default;
}

class BoltStreak {
public:
    BoltStreak(openrac::Mod& mod, const Disc& disc)
        : mod_(mod), disc_(disc), damage_(damage_setting(mod)), curve_(curve_setting(mod)) {}

    ~BoltStreak() { restore_hud(); }

    int multiplier() const {
        if (!active_) return 0;
        const auto memory = mod_.memory();
        const int health = memory.read<int>(hero_ + kHealth);
        if (health <= 0) return 1;
        if (memory.read<std::uint8_t>(save_ + kStreak) < streak_.native_multiplier()) {
            if (damage_ == Damage::Default) return 1;
            if (damage_ == Damage::Lenient && last_health_ > health) {
                return std::max(1, streak_.multiplier - 1);
            }
        }
        return streak_.multiplier;
    }

    void save_changed() {
        active_ = false;
        cycle_ = 0;
        last_health_ = 0;
        streak_ = {};
        update_hud(1);
    }

    void settings_changed() {
        damage_ = damage_setting(mod_);
        curve_ = curve_setting(mod_);
        if (active_ && streak_.multiplier > bolt_streak::rules(curve_, cycle_).cap) {
            streak_ = {bolt_streak::rules(curve_, cycle_).cap, 0};
            mirror();
        }
    }

    void tick() {
        const auto play = mod_.play_status();
        if ((damage_ == Damage::Default && curve_ == Curve::Default)
            || play.phase < OPENRAC_PLAY_PLAYING) {
            active_ = false;
            update_hud(1);
            return;
        }

        save_ = mod_.relocate(disc_.save);
        hero_ = mod_.relocate(disc_.hero);
        const auto memory = mod_.memory();
        const int cycle = memory.read<std::int16_t>(save_ + kCycle);
        if (cycle <= 0) {
            active_ = false;
            cycle_ = cycle;
            update_hud(1);
            return;
        }

        const int health = memory.read<int>(hero_ + kHealth);
        const int native = memory.read<std::uint8_t>(save_ + kStreak);
        const bool reloaded = loads_ != play.loads;
        if (cycle != cycle_ || native == 0 || (reloaded && play.level == level_)
            || (!active_ && native != streak_.native_multiplier())) {
            streak_ = {std::clamp(native, 1, 20),
                       4 * memory.read<std::uint8_t>(save_ + kProgress)};
        }
        if (health <= 0) {
            if (active_ || (last_health_ > 0 && !reloaded)) {
                streak_ = {};
                mirror();
            }
        } else if (native && native < streak_.native_multiplier()) {
            reconcile_contact(health);
        } else if (native > streak_.native_multiplier()) {
            // The game can also count a bestiary event outside SetDeathBits.
            streak_ = {native, 4 * memory.read<std::uint8_t>(save_ + kProgress)};
        }
        active_ = health > 0 && native > 0;
        cycle_ = cycle;
        level_ = play.level;
        loads_ = play.loads;
        last_health_ = health;
        update_hud(active_ ? streak_.multiplier : 1);
    }

    template<class Original>
    void hit(Original original, int amount) {
        if (!active_) {
            original(amount);
            return;
        }
        const auto memory = mod_.memory();
        const int before = memory.read<int>(hero_ + kHealth);
        original(amount);
        const int after = memory.read<int>(hero_ + kHealth);
        if (after <= 0) {
            streak_ = {};
            mirror();
            active_ = false;
        } else if (after < before) {
            streak_.hit(damage_);
            mirror();
        }
        last_health_ = after;
    }

    template<class Original>
    void death(Original original, gaddr moby, int flags, int maximum) {
        if (!active_ || !memory_enemy(moby)) {
            original(moby, flags, maximum);
            return;
        }

        const auto memory = mod_.memory();
        const int health = memory.read<int>(hero_ + kHealth);
        if (health <= 0) {
            streak_ = {};
            mirror();
            active_ = false;
            original(moby, flags, maximum);
            return;
        }
        if (memory.read<std::uint8_t>(save_ + kStreak) < streak_.native_multiplier()) {
            reconcile_contact(health);
        }
        const auto rule = bolt_streak::rules(curve_, cycle_);
        const bool accelerated = rule.credit_per_kill != 4;
        const int native = memory.read<std::uint8_t>(save_ + kStreak);
        const bool world = memory.read<int>(mod_.relocate(disc_.reward_slot)) == -1;
        const bool count = accelerated && world && bestiary_enemy(moby);
        const Drop outer = drop_;
        if (world && native && streak_.multiplier > native) {
            drop_ = {moby, native, streak_.multiplier, memory.read<int>(save_ + kReserve)};
        }
        original(moby, flags, maximum);
        drop_ = outer;

        if (count && active_) {
            streak_.kill(rule);
            mirror();
        } else if (!accelerated && active_) {
            const int next = memory.read<std::uint8_t>(save_ + kStreak);
            if (next) streak_ = {next, 4 * memory.read<std::uint8_t>(save_ + kProgress)};
        }
    }

    template<class Original>
    void burst(Original original, gaddr moby, int low, int high,
               int flags, int surface, int maximum, float height) {
        if (drop_.moby && moby == drop_.moby && high > 0) {
            const auto memory = mod_.memory();
            const int repaid = std::max(0, drop_.reserve - memory.read<int>(save_ + kReserve));
            // Stock computes both ends around enemy bolts plus reserve repayment. Old credit
            // already earned its multiplier. Only increase the enemy part of this drop.
            const std::int64_t center = (static_cast<std::int64_t>(low) + high) / 2;
            const auto fresh = std::max<std::int64_t>(0, center - repaid);
            const auto scaled = std::min<std::int64_t>(
                100'000'000, fresh * drop_.streak / drop_.native_streak + repaid);
            const auto spread = scaled >= 8 ? scaled / 4 : (scaled >= 2 ? 1 : 0);
            low = static_cast<int>(scaled - spread);
            high = static_cast<int>(scaled + spread);
            if (maximum > 0) {
                maximum = static_cast<int>(std::min<std::int64_t>(
                    64, (static_cast<std::int64_t>(maximum) * drop_.streak
                         + drop_.native_streak - 1) / drop_.native_streak));
            }
        }
        original(moby, low, high, flags, surface, maximum, height);
    }

private:
    void reconcile_contact(int health) {
        // Some contact paths reset the bytes before or outside AdjustPlayerHealth.
        if (damage_ == Damage::Default || (last_health_ > 0 && health < last_health_)) {
            streak_.hit(damage_);
        }
        last_health_ = health;
        mirror();
    }

    bool memory_enemy(gaddr moby) const {
        const auto memory = mod_.memory();
        if (memory.read<std::int16_t>(moby + 0xB2u) < 0
            || (memory.read<std::uint8_t>(moby + 0xB1u) & 0x80u)) return false;
        const auto klass = memory.read<gaddr>(moby + 0x24u);
        return klass && memory.read<std::int16_t>(klass + 0x46u) == 5;
    }

    bool bestiary_enemy(gaddr moby) const {
        const auto memory = mod_.memory();
        const auto id = memory.read<std::int16_t>(moby + 0xAAu);
        const auto rows = memory.read<std::array<std::array<std::int16_t, 12>, 64>>(
            mod_.relocate(disc_.bestiary));
        for (const auto& row : rows) {
            if (std::find(row.begin(), row.begin() + 4, id) != row.begin() + 4) return true;
        }
        return false;
    }

    void mirror() const {
        const auto memory = mod_.memory();
        memory.write<std::uint8_t>(save_ + kStreak, static_cast<std::uint8_t>(streak_.native_multiplier()));
        memory.write<std::uint8_t>(save_ + kProgress, static_cast<std::uint8_t>(streak_.native_progress()));
    }

    void update_hud(int multiplier) {
        if (multiplier <= 20) {
            restore_hud();
            return;
        }
        const auto address = mod_.relocate(disc_.hud_format);
        if (address != hud_address_) restore_hud();
        if (!address) return;
        const auto memory = mod_.memory();
        const auto current = memory.read<bolt_streak::HudFormat>(address);
        if (current != bolt_streak::stock_hud_format &&
            (address != hud_address_ || current != hud_text_)) return;

        // The panel formats this string itself. Only change its text, so it keeps the
        // game's font, position and fade without exposing an extended value to gameplay.
        const auto text = bolt_streak::hud_format(multiplier);
        if (text != current) memory.write(address, text);
        hud_address_ = address;
        hud_text_ = text;
    }

    void restore_hud() {
        if (!hud_address_) return;
        const auto memory = mod_.memory();
        if (memory.read<bolt_streak::HudFormat>(hud_address_) == hud_text_) {
            memory.write(hud_address_, bolt_streak::stock_hud_format);
        }
        hud_address_ = 0;
    }

    openrac::Mod& mod_;
    const Disc& disc_;
    Damage damage_;
    Curve curve_;
    bolt_streak::Streak streak_;
    Drop drop_;
    gaddr save_ = 0;
    gaddr hero_ = 0;
    int cycle_ = 0;
    int level_ = -1;
    std::uint32_t loads_ = 0;
    int last_health_ = 0;
    gaddr hud_address_ = 0;
    bolt_streak::HudFormat hud_text_{};
    bool active_ = false;
};

std::weak_ptr<BoltStreak> loaded_streak;

void install_hooks(openrac::Mod& mod, const Disc& disc) {
    auto state = std::make_shared<BoltStreak>(mod, disc);
    mod.on_frame([state] { state->tick(); });
    mod.on_save_data([state] { state->save_changed(); });
    mod.on_settings([state] { state->settings_changed(); });
    mod.hook<openrac::rac2::Hero_TakeDamage>(
        [state](auto original, int amount) { state->hit(original, amount); });
    mod.hook<openrac::rac2::Moby_RecordDeath>(
        [state](auto original, auto... args) { state->death(original, args...); });
    mod.hook<openrac::rac2::Bolt_SpawnPickups>(
        [state](auto original, auto... args) { state->burst(original, args...); });
    loaded_streak = state;
}

void load(openrac::Mod& mod) {
    if (mod.game() == "rac2") {
        for (const auto& disc : kDiscs) {
            if (mod.version() == disc.version) {
                install_hooks(mod, disc);
                return;
            }
        }
    }
    throw openrac::Error("Challenge Bolt Streak requires Going Commando PAL or NTSC-U");
}

} // namespace

#if defined(_WIN32)
extern "C" OPENRAC_MOD_EXPORT int rac2_challenge_bolt_streak_multiplier_v1() {
    const auto state = loaded_streak.lock();
    return state ? state->multiplier() : 0;
}
#endif

OPENRAC_MOD_CPP("rac2-challenge-bolt-streak", "0.2.1", load)
