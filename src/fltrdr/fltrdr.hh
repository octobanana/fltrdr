#ifndef FLTRDR_HH
#define FLTRDR_HH

#include "ob/timer.hh"
#include "ob/text.hh"
#include "ob/term.hh"
namespace aec = OB::Term::ANSI_Escape_Codes;

#include <cstddef>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

class Fltrdr
{
public:

  // current rendered line
  struct Line
  {
    std::string prev {};
    std::string curr {};
    std::string next {};
  };

  Fltrdr() = default;

  void init();
  bool parse(std::istream& input);

  Fltrdr& screen_size(std::size_t const width, std::size_t const height);

  bool eof();

  void begin();
  void end();

  OB::Text buf_prev(std::size_t offset = 0);
  OB::Text buf_next(std::size_t offset = 0);

  void set_focus_point();

  void set_line(std::size_t offset = 0);
  Line get_line();

  int get_wait();

  void set_index(std::size_t i);
  std::size_t get_index();

  int get_wpm();
  void set_wpm(int const i);
  void inc_wpm();
  void dec_wpm();

  void calc_wpm_avg();
  std::string get_stats();

  void set_show_line(bool const val);
  bool get_show_line();

  void set_show_prev(int const val);
  int get_show_prev();

  void set_show_next(int const val);
  int get_show_next();

  std::size_t progress();

  OB::Text word();
  void current_word();
  bool prev_word();
  bool next_word();

  void prev_sentence();
  void next_sentence();

  void prev_chapter();
  void next_chapter();

  bool search_next();
  bool search_prev();
  bool search(std::string const& rx, bool forward);

  void reset_timer();
  void reset_wpm_avg();

  OB::Timer timer;

private:

  struct Ctx
  {
    // current terminal size
    std::size_t width {0};
    std::size_t height {0};

    // minimum terminal size
    std::size_t width_min {20};

    // current word focus point
    double const focus {0.25};
    std::size_t focus_point {0};

    // current word display width in columns before focus point
    std::size_t prefix_width {0};

    // text buffer
    std::string str;
    OB::Text text;

    // current rendered line
    Line line;

    // text position of leading space to current word
    std::size_t pos {0};

    // word index
    std::size_t index {1};

    // min word index
    std::size_t index_min {1};

    // max word index
    std::size_t index_max {1};

    // current word
    OB::Text word;

    OB::Text prev;
    OB::Text next;

    // words per minute
    int const wpm_diff {10};
    int const wpm_min {60};
    int const wpm_max {1200};
    int wpm {250};
    int wpm_avg {0};
    int wpm_count {0};
    int wpm_total {0};

    // wait time in milliseconds
    int ms {0};

    // toggle prev and next buffer surrounding current word in line
    bool show_line {false};
    int show_prev {1};
    int show_next {1};
    int show_min {0};
    int show_max {60};

    struct Search
    {
      OB::Regex it;
      std::string rx;
      bool forward {true};
    } search;

    // sentence end characters
    OB::UString sentence_end {".!?"};
  } _ctx;

  bool search_forward();
  bool search_backward();
};

#endif // FLTRDR_HH
