// shared/ComputerWidgets.hpp
// ─────────────────────────────────────────────────────────────────────────────
// Shared widget classes used by BOTH the standalone Workshop Computer plugin
// and the Workshop System plugin (via the computer section embedded in it).
//
// Design:
//  - No dependency on a specific Module subclass type.
//  - Widgets talk to the module through IComputerModule (defined below).
//  - Both WorkshopComputer.cpp and WorkshopSystem.cpp include this file.
//  - pluginInstance must be defined in the including translation unit.
//
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "rack.hpp"
#include "cards/CardRegistry.hpp"
#include "ExtendedMetadata.hpp"

using namespace rack;

// ── Module interface ─────────────────────────────────────────────────────────
// Any module that hosts a computer card slot implements this interface so the
// shared widgets can call into it without knowing the concrete type.
struct IComputerModule {
    virtual ~IComputerModule() = default;
    virtual void change_card(int idx) = 0;
    virtual int  get_active_card_idx() const = 0;
    virtual std::string get_active_card_id() const = 0;
    virtual int  get_utility_index(int slot) const = 0;  // slot 0 or 1
    virtual void set_utility_index(int slot, int index) = 0;
    virtual void set_pending_page_direction(int dir) = 0;
    virtual float get_switch_z_val() const = 0;
};

// ── Knobs ─────────────────────────────────────────────────────────────────────

struct WorkshopLargeKnob : componentlibrary::RoundKnob {
    WorkshopLargeKnob() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/largeKnob_dark.svg")));
        shadow->opacity = 0.0;
    }
};

struct WorkshopSmallKnob : componentlibrary::RoundKnob {
    WorkshopSmallKnob() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/smallKnob_dark.svg")));
        shadow->opacity = 0.0;
    }
};

// ── Port ─────────────────────────────────────────────────────────────────────

struct WorkshopComputerPort : PJ301MPort {
    WorkshopComputerPort() {
        shadow->opacity = 0.0;
    }
};

// ── Toggle Switch (3-position, hold-to-latch) ─────────────────────────────────
// Call setComputerModule() after construction to wire page navigation.

struct WorkshopToggleSwitch : app::SvgSwitch {
    IComputerModule* computerModule = nullptr;

    bool downPressed  = false;
    bool downLatched  = false;
    double pressTime  = 0.0;
    static constexpr double LATCH_SECONDS = 0.45;

    WorkshopToggleSwitch() {
        momentary = false;
        shadow->opacity = 0.0;
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switch_down.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switch_middle.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switch_up.svg")));
        box.size = Vec(24.f, 28.f);
    }

    void setComputerModule(IComputerModule* m) { computerModule = m; }

    void onButton(const event::Button& e) override {
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;

        auto* pq = getParamQuantity();
        if (!pq) return;

        float midY = box.size.y / 2.f;

        if (e.action == GLFW_PRESS) {
            if (e.pos.y < midY) {
                // UP zone: toggle latch
                pq->setValue(pq->getValue() == 2.f ? 1.f : 2.f);
                downPressed = false;
                downLatched = false;
            } else {
                // DOWN zone
                if (downLatched) {
                    pq->setValue(1.f);
                    downLatched = false;
                    downPressed = false;
                } else {
                    pq->setValue(0.f);
                    downPressed = true;
                    pressTime = glfwGetTime();
                }
            }
            e.consume(this);
        } else if (e.action == GLFW_RELEASE) {
            if (downPressed) {
                double held = glfwGetTime() - pressTime;
                if (held >= LATCH_SECONDS) {
                    if (computerModule) computerModule->set_pending_page_direction(-1);
                    downLatched = true;
                } else {
                    if (computerModule) computerModule->set_pending_page_direction(1);
                    pq->setValue(1.f);
                    downLatched = false;
                }
                downPressed = false;
            }
            e.consume(this);
        }
    }

    void draw(const DrawArgs& args) override {
        float svgW = 24.f, svgH = 28.f;
        float offX = (box.size.x - svgW) / 2.f;
        float offY = (box.size.y - svgH) / 2.f;
        nvgSave(args.vg);
        nvgTranslate(args.vg, offX, offY);
        float scale = 1.125f;
        nvgTranslate(args.vg, svgW / 2.f * (1.f - scale), svgH / 2.f * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }

    void onDragStart(const DragStartEvent& e) override {}
    void onDragEnd(const DragEndEvent& e) override {}
    void onDragMove(const DragMoveEvent& e) override {}
};

// ── Reset Button ──────────────────────────────────────────────────────────────

struct WorkshopResetButton : Widget {
    IComputerModule* computerModule = nullptr;
    bool active = false;
    bool hover  = false;

    WorkshopResetButton() { box.size = Vec(15.f, 15.f); }

    void setComputerModule(IComputerModule* m) { computerModule = m; }

    void onEnter(const event::Enter& e) override { hover = true; }
    void onLeave(const event::Leave& e) override { hover = false; }

    void onButton(const event::Button& e) override {
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
        if (e.action == GLFW_PRESS) {
            active = true;
            e.consume(this);
        } else if (e.action == GLFW_RELEASE) {
            if (active) {
                active = false;
                if (computerModule && computerModule->get_active_card_idx() != -1) {
                    computerModule->change_card(computerModule->get_active_card_idx());
                }
                e.consume(this);
            }
        }
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 7.5f, 7.5f, 5.67f);
        if (active) {
            nvgFillColor(args.vg, nvgRGBA(255, 0, 0, 180));
        } else if (hover) {
            nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 80));
        } else {
            nvgRestore(args.vg);
            return;
        }
        nvgFill(args.vg);
        nvgRestore(args.vg);
    }
};

// ── Program Card Widget (card slot: left/right-click to change card) ───────────

struct ProgramCardWidget : Widget {
    IComputerModule* computerModule = nullptr;
    ui::Tooltip* tooltip = nullptr;

    std::shared_ptr<Svg> svgBlank;
    std::shared_ptr<Svg> svgTuring;
    std::shared_ptr<Svg> svgReverb;
    std::shared_ptr<Svg> svgMidi;

    ProgramCardWidget() {
        svgBlank  = Svg::load(asset::plugin(pluginInstance, "res/card_blank.svg"));
        svgTuring = Svg::load(asset::plugin(pluginInstance, "res/card_turing.svg"));
        svgReverb = Svg::load(asset::plugin(pluginInstance, "res/card_reverb.svg"));
        svgMidi   = Svg::load(asset::plugin(pluginInstance, "res/card_midi.svg"));
    }

    ~ProgramCardWidget() override {
        if (tooltip) {
            if (APP && APP->scene) {
                APP->scene->removeChild(tooltip);
            }
            delete tooltip;
        }
    }

    void onEnter(const event::Enter& e) override {
        Widget::onEnter(e);
        if (!tooltip && computerModule && APP && APP->scene) {
            std::string card_id = computerModule->get_active_card_id();
            const auto* meta = ExtendedMetadata::get_card_metadata(card_id);
            if (meta) {
                tooltip = new ui::Tooltip();
                std::string t = meta->name + "\n" + meta->description;
                if (!meta->manual.empty() && meta->manual != meta->description) {
                    t += "\n\nManual:\n" + meta->manual;
                }
                tooltip->text = t;
                APP->scene->addChild(tooltip);
            }
        }
    }

    void onLeave(const event::Leave& e) override {
        Widget::onLeave(e);
        if (tooltip) {
            if (APP && APP->scene) {
                APP->scene->removeChild(tooltip);
            }
            delete tooltip;
            tooltip = nullptr;
        }
    }

    void step() override {
        Widget::step();
        if (tooltip && computerModule) {
            std::string card_id = computerModule->get_active_card_id();
            const auto* meta = ExtendedMetadata::get_card_metadata(card_id);
            if (meta) {
                std::string t = meta->name + "\n" + meta->description;
                if (!meta->manual.empty() && meta->manual != meta->description) {
                    t += "\n\nManual:\n" + meta->manual;
                }
                tooltip->text = t;
            } else {
                tooltip->text = "";
            }
        }
    }

    void setComputerModule(IComputerModule* m) { computerModule = m; }

    // ── Submenu: a numbered group of cards ───────────────────────────────────
    struct CardGroupSubmenuItem : MenuItem {
        IComputerModule* computerModule;
        int min_num, max_num;

        Menu* createChildMenu() override {
            Menu* menu = new Menu();
            struct CardItem : MenuItem {
                IComputerModule* computerModule;
                int card_idx;
                void onAction(const event::Action& e) override {
                    computerModule->change_card(card_idx);
                }
            };
            for (size_t i = 0; i < g_card_registry.size(); i++) {
                if (!g_card_registry[i].visible) continue;
                int num = -1;
                try { num = std::stoi(g_card_registry[i].number); }
                catch (...) { continue; }
                if (num >= min_num && num <= max_num) {
                    auto* item = new CardItem();
                    item->text = "[#" + g_card_registry[i].number + "] " + g_card_registry[i].name;
                    item->computerModule = computerModule;
                    item->card_idx = (int)i;
                    item->rightText = (computerModule->get_active_card_idx() == (int)i) ? "✔" : "";
                    menu->addChild(item);
                }
            }
            return menu;
        }
    };

    void onButton(const event::Button& e) override {
        if (e.action != GLFW_PRESS || !computerModule) return;
        if (e.button != GLFW_MOUSE_BUTTON_LEFT && e.button != GLFW_MOUSE_BUTTON_RIGHT) return;

        e.consume(this);
        Menu* menu = createMenu();
        menu->addChild(createMenuLabel("Load Program Cartridge"));

        struct CardItem : MenuItem {
            IComputerModule* computerModule;
            int card_idx;
            void onAction(const event::Action& e) override {
                computerModule->change_card(card_idx);
            }
        };

        auto* unloadItem = new CardItem();
        unloadItem->text = "Unload Card (Empty Slot)";
        unloadItem->computerModule = computerModule;
        unloadItem->card_idx = -1;
        unloadItem->rightText = (computerModule->get_active_card_idx() == -1) ? "✔" : "";
        menu->addChild(unloadItem);
        menu->addChild(new MenuSeparator());

        auto is_active_in_range = [&](int min_n, int max_n) -> bool {
            int idx = computerModule->get_active_card_idx();
            if (idx < 0 || idx >= (int)g_card_registry.size()) return false;
            try { int num = std::stoi(g_card_registry[idx].number); return num >= min_n && num <= max_n; }
            catch (...) { return false; }
        };

        auto has_visible_cards_in_range = [&](int min_n, int max_n) -> bool {
            for (size_t i = 0; i < g_card_registry.size(); i++) {
                if (!g_card_registry[i].visible) continue;
                try {
                    int num = std::stoi(g_card_registry[i].number);
                    if (num >= min_n && num <= max_n) return true;
                } catch (...) {}
            }
            return false;
        };

        auto make_group = [&](const char* label, int lo, int hi) {
            if (!has_visible_cards_in_range(lo, hi)) return;
            auto* g = new CardGroupSubmenuItem();
            g->text = label;
            g->computerModule = computerModule;
            g->min_num = lo;
            g->max_num = hi;
            g->rightText = is_active_in_range(lo, hi) ? "✔ ➔" : "➔";
            menu->addChild(g);
        };

        make_group("Cards #00 - #19",  0, 19);
        make_group("Cards #20 - #39", 20, 39);
        make_group("Cards #40 - #59", 40, 59);
        make_group("Cards #60 - #99", 60, 99);

        int active_idx = computerModule->get_active_card_idx();
        if (active_idx >= 0 && active_idx < (int)g_card_registry.size() && g_card_registry[active_idx].id == "utility_pair") {
            menu->addChild(new MenuSeparator());
            menu->addChild(createMenuLabel("Utility Pair Selection"));

            struct UtilitySubmenuItem : MenuItem {
                IComputerModule* computerModule;
                int channel; // 0 = Left, 1 = Right

                Menu* createChildMenu() override {
                    Menu* menu = new Menu();
                    static const std::string UTILITIES[24] = {
                        "Attenuverter", "Bernoulli Gate", "Bitcrusher", "Chords",
                        "Chorus", "Clock Divider", "Cross Switch", "CV Mixer",
                        "Delay", "Euclidean Rhythms", "Glitch", "Karplus-Strong",
                        "Low Pass Gate", "Max/Rectify", "Quantiser", "Sample & Hold",
                        "Slopes Plus", "Slow LFO", "Super Saw", "Turing 185",
                        "VCA", "VCO", "Wavefolder", "Window Comparator"
                    };

                    struct UtilItem : MenuItem {
                        IComputerModule* computerModule;
                        int channel;
                        int index;
                        void onAction(const event::Action& e) override {
                            computerModule->set_utility_index(channel, index);
                        }
                    };

                    for (int i = 0; i < 24; i++) {
                        UtilItem* item = new UtilItem();
                        item->text = UTILITIES[i];
                        item->computerModule = computerModule;
                        item->channel = channel;
                        item->index = i;
                        item->rightText = (computerModule->get_utility_index(channel) == i) ? "✔" : "";
                        menu->addChild(item);
                    }
                    return menu;
                }
            };

            UtilitySubmenuItem* leftItem = new UtilitySubmenuItem();
            leftItem->text = "Left Utility Channel";
            leftItem->computerModule = computerModule;
            leftItem->channel = 0;
            menu->addChild(leftItem);

            UtilitySubmenuItem* rightItem = new UtilitySubmenuItem();
            rightItem->text = "Right Utility Channel";
            rightItem->computerModule = computerModule;
            rightItem->channel = 1;
            menu->addChild(rightItem);
        }
    }

    void draw(const DrawArgs& args) override {
        if (!computerModule || computerModule->get_active_card_idx() == -1) return;

        float w = box.size.x, h = box.size.y;
        nvgSave(args.vg);
        nvgIntersectScissor(args.vg, 0, 0, w, h);

        std::shared_ptr<Svg> activeSvg = svgBlank;
        std::string active_id = computerModule->get_active_card_id();
        if (active_id == "turing_machine") activeSvg = svgTuring;
        else if (active_id == "reverb")     activeSvg = svgReverb;
        else if (active_id == "simple_midi") activeSvg = svgMidi;

        nvgSave(args.vg);
        float full_h = w * (54.f / 32.f);
        nvgScale(args.vg, w / 32.f, full_h / 54.f);

        if (activeSvg && activeSvg->handle) {
            svgDraw(args.vg, activeSvg->handle);
        }

        if (activeSvg == svgBlank) {
            int idx = computerModule->get_active_card_idx();
            if (idx >= 0 && idx < (int)g_card_registry.size()) {
                std::string name = g_card_registry[idx].name;
                std::string num  = g_card_registry[idx].number;

                // Utility Pair: show selected pair names
                if (g_card_registry[idx].id == "utility_pair") {
                    static const std::string UTILITIES_SHORT[24] = {
                        "Attn","Bern","Crsh","Chrd","Chor","C.Div","Cross","CVMx",
                        "Dly","Eucl","Gltc","K-S","LPG","Max","Qnt","S&H",
                        "Slp","LFO","Saw","T185","VCA","VCO","Fold","W.Cmp"
                    };
                    int l = computerModule->get_utility_index(0);
                    int r = computerModule->get_utility_index(1);
                    if (l >= 0 && l < 24 && r >= 0 && r < 24)
                        name = UTILITIES_SHORT[l] + " " + UTILITIES_SHORT[r];
                }

                for (auto& c : name) c = std::toupper(c);

                // Split name into 2 lines at the space closest to the middle of the string
                std::string line1 = name;
                std::string line2 = "";
                size_t first_space = name.find(' ');
                if (first_space != std::string::npos) {
                    size_t best_space = first_space;
                    int min_diff = std::abs((int)first_space - (int)(name.length() - 1 - first_space));
                    size_t next_space = name.find(' ', first_space + 1);
                    while (next_space != std::string::npos) {
                        int diff = std::abs((int)next_space - (int)(name.length() - 1 - next_space));
                        if (diff < min_diff) {
                            min_diff = diff;
                            best_space = next_space;
                        }
                        next_space = name.find(' ', next_space + 1);
                    }
                    line1 = name.substr(0, best_space);
                    line2 = name.substr(best_space + 1);
                }

                std::shared_ptr<window::Font> font;
#ifdef __APPLE__
                font = APP->window->loadFont("/System/Library/Fonts/Supplemental/Arial Bold.ttf");
                if (!font) font = APP->window->loadFont("/System/Library/Fonts/HelveticaNeue.ttc");
                if (!font) font = APP->window->loadFont("/System/Library/Fonts/Helvetica.ttc");
#endif
                if (!font) font = APP->window->loadFont(asset::system("res/fonts/Roboto-Bold.ttf"));
                if (font) nvgFontFaceId(args.vg, font->handle);

                nvgFillColor(args.vg, nvgRGBA(227, 214, 158, 220));
                nvgFontSize(args.vg, 9.0f);
                nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
                for (float dy : {-0.1f, 0.1f, 0.f}) for (float dx : {-0.1f, 0.1f, 0.f})
                    nvgText(args.vg, 2.5f + dx, 3.5f + dy, num.c_str(), NULL);

                nvgFillColor(args.vg, nvgRGBA(227, 214, 158, 255));
                nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

                auto draw_rotated_word = [&](const std::string& word, float x) {
                    if (word.empty()) return;
                    float fs = 11.0f;
                    float approx_w = word.length() * fs * 0.7f;
                    if (approx_w > 38.f) fs = 38.f / (word.length() * 0.7f);
                    if (fs < 5.f) fs = 5.f;
                    nvgFontSize(args.vg, fs);
                    nvgSave(args.vg);
                    nvgTranslate(args.vg, x, 28.5f);
                    nvgRotate(args.vg, M_PI / 2.f);
                    for (float dy : {-0.15f, 0.15f, 0.f}) for (float dx : {-0.15f, 0.15f, 0.f})
                        nvgText(args.vg, dx, dy, word.c_str(), NULL);
                    nvgText(args.vg, 0.f, 0.f, word.c_str(), NULL);
                    nvgRestore(args.vg);
                };

                if (!line2.empty()) {
                    draw_rotated_word(line1, 22.7f); // First line goes on the right column (reads first when tilted left)
                    draw_rotated_word(line2, 13.7f); // Second line goes on the left column (reads second when tilted left)
                } else {
                    draw_rotated_word(line1, 18.2f); // Single line is centered
                }
            }
        }

        nvgRestore(args.vg); // restore scaled transform
        nvgRestore(args.vg); // restore scissor
    }
};
