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
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

namespace OB
{

Readline& Readline::style(std::string const& style)
{
  _style.input = style;

  return *this;
}

Readline& Readline::prompt(std::string const& str, std::string const& style)
{
  _prompt.str = str;
  _style.prompt = style;
  _prompt.fmt = aec::wrap(str, style);

  return *this;
}

void Readline::refresh()
{
  _prompt.lhs = _prompt.fmt;
  _prompt.rhs.clear();

  if (_input.str.cols() + 2 > _width)
  {
    std::size_t pos {_input.off + _input.idx - 1};
    std::size_t cols {0};

    for (; pos != OB::Text::String::npos && cols < _width - 2; --pos)
    {
      cols += _input.str.at(pos).cols;
    }

    if (pos == OB::Text::String::npos)
    {
      pos = 0;
    }

    std::size_t end {_input.off + _input.idx - pos};
    _input.fmt.str(_input.str.substr(pos, end));

    if (_input.fmt.cols() > _width - 2)
    {
      while (_input.fmt.cols() > _width - 2)
      {
        _input.fmt.erase(0, 1);
      }

      _input.cur = _input.fmt.cols();

      if (_input.cur == OB::Text::String::npos)
      {
        _input.cur = 0;
      }

      _prompt.lhs = aec::wrap("<", _style.prompt);
    }
    else
    {
      _input.cur = _input.fmt.cols();

      if (_input.cur == OB::Text::String::npos)
      {
        _input.cur = 0;
      }

      while (_input.fmt.cols() <= _width - 2)
      {
        _input.fmt.append(std::string(_input.str.at(end++).str));
      }

      _input.fmt.erase(_input.fmt.size() - 1, 1);
    }

    if (_input.off + _input.idx < _input.str.size())
    {
      _prompt.rhs = aec::wrap(
        OB::String::repeat(_width - _input.fmt.cols() - 2, " ") + ">",
        _style.prompt);
    }
  }
  else
  {
    _input.fmt = _input.str;
    _input.cur = _input.fmt.cols(0, _input.idx);

    if (_input.cur == OB::Text::String::npos)
    {
      _input.cur = 0;
    }
  }

  std::cout
  << aec::cursor_hide
  << aec::cr
  << aec::erase_line
  << _style.input
  << OB::String::repeat(_width, " ")
  << aec::cr
  << _prompt.lhs
  << _style.input
  << _input.fmt
  << aec::clear
  << _prompt.rhs
  << aec::cursor_set(_input.cur + 2, _height)
  << aec::cursor_show
  << std::flush;
}

std::string Readline::operator()(bool& is_running)
{
  // update width and height of terminal
  OB::Term::size(_width, _height);

  // reset input struct
  _input = {};

  // input key as 32-bit char
  char32_t ch {0};

  // input key as utf8 string
  // contains 1-4 bytes
  std::string utf8;

  bool loop {true};
  bool clear_input {false};
  auto wait {std::chrono::milliseconds(50)};

  std::cout
  << aec::cr
  << aec::erase_line
  << _style.input
  << OB::String::repeat(_width, " ")
  << aec::cr
  << _prompt.fmt
  << std::flush;

  while (loop && is_running)
  {
    std::this_thread::sleep_for(wait);

    while ((ch = OB::Term::get_key(&utf8)) > 0)
    {
      switch (ch)
      {
        case OB::Term::Key::escape:
        {
          // exit the command prompt
          loop = false;
          clear_input = true;

          break;
        }

        case OB::Term::Key::tab:
        {
          // TODO add tab completion

          break;
        }

        case OB::Term::ctrl_key('c'):
        {
          // exit the main event loop
          is_running = false;
          clear_input = true;

          break;
        }

        case OB::Term::Key::newline:
        {
          // submit the input string
          loop = false;

          break;
        }

        case OB::Term::Key::up:
        case OB::Term::ctrl_key('p'):
        {
          hist_prev();

          break;
        }

        case OB::Term::Key::down:
        case OB::Term::ctrl_key('n'):
        {
          hist_next();

          break;
        }

        case OB::Term::Key::right:
        case OB::Term::ctrl_key('f'):
        {
          curs_right();

          break;
        }

        case OB::Term::Key::left:
        case OB::Term::ctrl_key('b'):
        {
          curs_left();

          break;
        }

        case OB::Term::Key::end:
        case OB::Term::ctrl_key('e'):
        {
          curs_end();

          break;
        }

        case OB::Term::Key::home:
        case OB::Term::ctrl_key('a'):
        {
          curs_begin();

          break;
        }

        case OB::Term::Key::delete_:
        case OB::Term::ctrl_key('d'):
        {
          loop = edit_delete();

          break;
        }

        case OB::Term::Key::backspace:
        case OB::Term::ctrl_key('h'):
        {
          loop = edit_backspace();

          break;
        }

        default:
        {
          if (ch < 0xF0000 && (ch == OB::Term::Key::space || OB::Text::is_graph(static_cast<std::int32_t>(ch))))
          {
            edit_insert(utf8);
          }


          break;
        }
      }
    }
  }

  auto res = normalize(_input.str);

  hist_push(res);

  if (clear_input)
  {
    res.clear();
  }

  return res;
}

void Readline::curs_begin()
{
  // move cursor to start of line

  if (_input.idx || _input.off)
  {
    _input.idx = 0;
    _input.off = 0;

    refresh();
  }
}

void Readline::curs_end()
{
  // move cursor to end of line

  if (_input.str.empty())
  {
    return;
  }

  if (_input.off + _input.idx < _input.str.size())
  {
    if (_input.str.cols() + 2 > _width)
    {
      _input.off = _input.str.size() - _width + 2;
      _input.idx = _width - 2;
    }
    else
    {
      _input.idx = _input.str.size();
    }

    refresh();
  }
}

void Readline::curs_left()
{
  // move cursor left

  if (_input.off || _input.idx)
  {
    if (_input.off)
    {
      --_input.off;
    }
    else
    {
      --_input.idx;
    }

    refresh();
  }
}

void Readline::curs_right()
{
  // move cursor right

  if (_input.off + _input.idx < _input.str.size())
  {
    if (_input.idx + 2 < _width)
    {
      ++_input.idx;
    }
    else
    {
      ++_input.off;
    }

    refresh();
  }
}

void Readline::edit_insert(std::string const& str)
{
  // insert or append char to input buffer

  _input.str.insert(_input.off + _input.idx, str);

  if (_input.idx + 2 < _width)
  {
    ++_input.idx;
  }
  else
  {
    ++_input.off;
  }

  refresh();

  _history.idx = _history.val.size();
}

bool Readline::edit_delete()
{
  // erase char under cursor

  if (_input.str.empty())
  {
    _input.str.clear();

    return false;
  }

  if (_input.off + _input.idx < _input.str.size())
  {
    if (_input.idx + 2 < _width)
    {
      _input.str.erase(_input.off + _input.idx, 1);
    }
    else
    {
      _input.str.erase(_input.idx, 1);
    }

    refresh();

    _history.idx = _history.val.size();
  }
  else if (_input.off || _input.idx)
  {
    if (_input.off)
    {
      _input.str.erase(_input.off + _input.idx - 1, 1);
      --_input.off;
    }
    else
    {
      --_input.idx;
      _input.str.erase(_input.idx, 1);
    }

    refresh();

    _history.idx = _history.val.size();
  }

  return true;
}

bool Readline::edit_backspace()
{
  // erase char behind cursor

  if (_input.str.empty())
  {
    _input.str.clear();

    return false;
  }

  _input.str.erase(_input.off + _input.idx - 1, 1);

  if (_input.off || _input.idx)
  {
    if (_input.off)
    {
      --_input.off;
    }
    else if (_input.idx)
    {
      --_input.idx;
    }

    refresh();

    _history.idx = _history.val.size();
  }

  return true;
}

void Readline::hist_prev()
{
  // cycle backwards in history

  if (_history.idx)
  {
    if (_history.idx == _history.val.size())
    {
      _input.buf = _input.str;
    }

    --_history.idx;
    _input.str = _history.val.at(_history.idx);

    if (_input.str.size() + 1 >= _width)
    {
      _input.off = _input.str.size() - _width + 2;
      _input.idx = _width - 2;
    }
    else
    {
      _input.off = 0;
      _input.idx = _input.str.size();
    }

    refresh();
  }
}

void Readline::hist_next()
{
  // cycle forwards in history

  if (_history.idx < _history.val.size())
  {
    ++_history.idx;
    if (_history.idx == _history.val.size())
    {
      _input.str = _input.buf;
    }
    else
    {
      _input.str = _history.val.at(_history.idx);
    }

    if (_input.str.size() + 1 >= _width)
    {
      _input.off = _input.str.size() - _width + 2;
      _input.idx = _width - 2;
    }
    else
    {
      _input.off = 0;
      _input.idx = _input.str.size();
    }

    refresh();
  }
}

void Readline::hist_push(std::string const& str)
{
  if (! str.empty() && ! (! _history.val.empty() && _history.val.back() == str))
  {
    if (auto pos = std::find(_history.val.begin(), _history.val.end(), str); pos != _history.val.end())
    {
      _history.val.erase(pos);
    }
    _history.val.emplace_back(str);
  }

  _history.idx = _history.val.size();
}

std::string Readline::hist_pop()
{
  if (_history.val.empty())
  {
    return {};
  }

  auto const res = _history.val.back();
  _history.val.pop_back();

  return res;
}

std::string Readline::normalize(std::string const& str) const
{
  // TODO add normalize toggle
  return OB::String::trim(str);

  // trim leading and trailing whitespace
  // collapse sequential whitespace
  // return OB::String::lowercase(std::regex_replace(
  //   OB::String::trim(str), std::regex("\\s+"),
  //   " ", std::regex_constants::match_not_null
  // ));
}

} // namespace OB
