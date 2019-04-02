#ifndef TUI_HH
#define TUI_HH

#include "fltrdr/fltrdr.hh"

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

  void config(std::string const& custom_path = {});
  Tui& init(fs::path const& path = {});
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

      int count_total {3};
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
      std::string bg {};

      std::string primary {};
      std::string secondary {};
      std::string background {aec::reverse};

      std::string border {aec::fg_white};

      std::string countdown {aec::reverse};

      std::string progress_bar {aec::fg_white};
      std::string progress_fill {};

      std::string prompt {};
      std::string prompt_status {};
      std::string success {aec::fg_green};
      std::string error {aec::fg_red};

      std::string word_primary {};
      std::string word_secondary {};
      std::string word_highlight {};
      std::string word_punct {};
      std::string word_quote {};
    } style;

    struct Sym
    {
      std::string border_top {" "};
      std::string border_top_mark {" "};

      std::string border_bottom {" "};
      std::string border_bottom_mark {"^"};

      std::string progress {"_"};
    } sym;
  } _ctx;
};

#endif // TUI_HH
