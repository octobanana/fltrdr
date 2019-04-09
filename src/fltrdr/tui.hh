#ifndef TUI_HH
#define TUI_HH

#include "fltrdr/fltrdr.hh"

#include "ob/color.hh"
#include "ob/readline.hh"
#include "ob/string.hh"
#include "ob/text.hh"
#include "ob/term.hh"
namespace aec = OB::Term::ANSI_Escape_Codes;

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <string>
#include <vector>
#include <sstream>
#include <utility>
#include <optional>

#include <filesystem>
namespace fs = std::filesystem;

class Tui
{
public:

  Tui();

  Tui& init(fs::path const& path = {});
  void base_config(fs::path const& path);
  void load_config(fs::path const& path);
  bool save_state();
  bool load_state();
  void load_hist_command(fs::path const& path);
  void load_hist_search(fs::path const& path);
  void run();

private:

  void get_input(int& wait);
  bool press_to_continue(std::string const& str = "ANY KEY", char32_t val = 0);

  std::optional<std::pair<bool, std::string>> command(std::string const& input);
  void command_prompt();

  void event_loop();
  int screen_size();

  void clear();
  void refresh();

  void draw();
  void draw_content();
  void draw_border_top();
  void draw_border_bottom();
  void draw_progress_bar();
  void draw_status();
  void draw_prompt_message();
  void draw_keybuf();

  void play();
  void pause();

  void set_wait();
  void set_status(bool success, std::string const& msg);

  void search_forward();
  void search_backward();

  OB::Term::Mode _term_mode;
  bool const _colorterm;
  OB::Readline _readline;
  OB::Readline _readline_search;
  Fltrdr _fltrdr;

  struct Ctx
  {
    struct File
    {
      // file to read from
      fs::path path;

      // file name
      std::string name;
    } file;

    // base config directory
    fs::path base_config;

    // current terminal size
    std::size_t width {0};
    std::size_t height {0};

    // minimum terminal size
    std::size_t width_min {20};
    std::size_t height_min {6};

    // output buffer
    std::ostringstream buf;

    // control when to exit the event loop
    bool is_running {true};

    // interval between reading a keypress
    int const input_interval {50};

    // horizontal offset from center
    std::size_t offset {0};
    int offset_value {2};

    struct State
    {
      bool play {false};

      int count_total {2};
      int count_down {0};
      bool counting_down {false};

      int wait {250};
      int refresh_rate {250};
    } state;

    // status
    struct Status
    {
      std::string str;
      std::string mode {"FLTRDR"};
    } status;

    // input key buffers
    OB::Text::Char32 key;
    std::vector<OB::Text::Char32> keys;

    // command prompt
    struct Prompt
    {
      std::string str;
      int count {0};
      int timeout {12};
    } prompt;

    struct Show
    {
      bool border_top {true};
      bool border_bottom {true};
      bool progress {true};
      bool status {true};
    } show;

    struct Style
    {
      OB::Color bg {OB::Color::Type::bg};

      OB::Color primary {OB::Color::Type::fg};
      OB::Color secondary {OB::Color::Type::fg};
      OB::Color background {"reverse", OB::Color::Type::bg};

      OB::Color border {"white", OB::Color::Type::fg};

      OB::Color countdown {"reverse", OB::Color::Type::bg};

      OB::Color progress_bar {"white", OB::Color::Type::fg};
      OB::Color progress_fill {OB::Color::Type::fg};

      OB::Color prompt {OB::Color::Type::fg};
      OB::Color prompt_status {OB::Color::Type::fg};
      OB::Color success {"green", OB::Color::Type::fg};
      OB::Color error {"red", OB::Color::Type::fg};

      OB::Color word_primary {OB::Color::Type::fg};
      OB::Color word_secondary {OB::Color::Type::fg};
      OB::Color word_highlight {OB::Color::Type::fg};
      OB::Color word_punct {OB::Color::Type::fg};
      OB::Color word_quote {OB::Color::Type::fg};
    } style;

    struct Sym
    {
      std::string border_top {" "};
      std::string border_top_mark {" "};

      std::string border_bottom {" "};
      std::string border_bottom_mark {"^"};

      std::string progress_bar {"_"};
      std::string progress_fill {"_"};
    } sym;
  } _ctx;
};

#endif // TUI_HH
