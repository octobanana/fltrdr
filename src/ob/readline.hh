#ifndef OB_READLINE_HH
#define OB_READLINE_HH

#include "ob/text.hh"
#include "ob/term.hh"
namespace aec = OB::Term::ANSI_Escape_Codes;

#include <cstdio>
#include <cstddef>
#include <cstdlib>

#include <string>
#include <vector>

namespace OB
{

class Readline
{
public:

  Readline() = default;

  Readline& prompt(std::string const& str, std::vector<std::string> const& style = {});

  std::string operator()(bool& is_running);

  void hist_push(std::string const& str);
  std::string hist_pop();

private:

  void refresh();

  void curs_begin();
  void curs_end();
  void curs_left();
  void curs_right();

  void edit_insert(std::string const& str);
  bool edit_delete();
  bool edit_backspace();

  void hist_prev();
  void hist_next();

  std::string normalize(std::string const& str) const;

  // width and height of the terminal
  std::size_t _width {0};
  std::size_t _height {0};

  struct Prompt
  {
    std::string lhs;
    std::string rhs;
    std::string fmt;
    std::string str {":"};
    std::vector<std::string> style;
  } _prompt;

  struct Input
  {
    std::size_t off {0};
    std::size_t idx {0};
    std::size_t cur {0};
    std::string buf;
    OB::Text::String str;
    OB::Text::String fmt;
  } _input;

  struct History
  {
    std::vector<std::string> val;
    std::size_t idx {0};
  } _history;
};

} // namespace OB

#endif // OB_READLINE_HH
