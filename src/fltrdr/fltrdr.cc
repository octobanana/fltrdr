#include "fltrdr/fltrdr.hh"

#include "ob/string.hh"
#include "ob/timer.hh"
#include "ob/term.hh"
namespace aec = OB::Term::ANSI_Escape_Codes;

#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdint>

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <stdexcept>
#include <regex>
#include <iterator>
#include <random>

void Fltrdr::init()
{
  _ctx.text.clear();
  _ctx.text.shrink_to_fit();

  _ctx.pos = 0;
  _ctx.index = 1;
  _ctx.wpm_avg = 0;
  _ctx.wpm_count = 0;
  _ctx.wpm_total = 0;
  _ctx.slow = false;
  _ctx.search.it = std::sregex_iterator();
}

bool Fltrdr::parse(std::istream& input)
{
  init();

  std::string word;
  std::size_t word_count {0};
  while (input >> std::ws >> word)
  {
    _ctx.text += " " + word;
    ++word_count;
  }

  if (word_count == 0)
  {
    _ctx.text = " fltrdr";
    ++word_count;
    _ctx.index_max = word_count;
    return false;
  }

  _ctx.index_max = word_count;
  return true;
}

bool Fltrdr::eof()
{
  return _ctx.index >= _ctx.index_max;
}

void Fltrdr::begin()
{
  set_index(_ctx.index_min);
}

void Fltrdr::end()
{
  set_index(_ctx.index_max);
}

Fltrdr& Fltrdr::screen_size(std::size_t const width, std::size_t const height)
{
  _ctx.width = width;
  _ctx.height = height;

  return *this;
}

std::string Fltrdr::buf_prev(std::size_t offset)
{
  if (! _ctx.pos)
  {
    return {};
  }

  auto const width = (_ctx.width / 2) - 1 - offset;

  auto pos = _ctx.pos + 1;
  int size {static_cast<int>(width - _ctx.focus_point)};
  auto count = size;

  while (pos && count)
  {
    --pos;
    --count;
  }

  size = size - count;
  if (size < 1)
  {
    return {};
  }

  auto const min = pos;

  if (_ctx.show_line)
  {
    return _ctx.text.substr(min, static_cast<std::size_t>(size));
  }

  pos = _ctx.pos;

  for (int i = 0; i < _ctx.show_prev; ++i)
  {
    pos = _ctx.text.rfind(" ", --pos);

    if (pos == std::string::npos || pos == 0)
    {
      break;
    }
  }

  auto const start = pos > min ? pos : min;
  auto const len = _ctx.pos + 1 - start < static_cast<std::size_t>(size) ?
    _ctx.pos + 1 - start : static_cast<std::size_t>(size);
  return _ctx.text.substr(start, len);
}

std::string Fltrdr::buf_next(std::size_t offset)
{
  auto pos = _ctx.text.find(" ", _ctx.pos + 1);
  auto const start = pos;

  if (pos == std::string::npos)
  {
    return {};
  }

  auto const width = (_ctx.width / 2) + 1 + offset;

  auto p = pos;
  int size {static_cast<int>(width - (_ctx.word.size() - _ctx.focus_point))};

  if (_ctx.width % 2 != 0)
  {
    ++size;
  }

  auto count = size;

  if (size < 1)
  {
    return {};
  }

  while (++count != size && ++p != _ctx.text.size() - 1);

  auto const max = static_cast<std::size_t>(size);

  if (_ctx.show_line)
  {
    return _ctx.text.substr(pos, max);
  }

  for (int i = 0; i < _ctx.show_next; ++i)
  {
    pos = _ctx.text.find(" ", ++pos);

    if (pos == std::string::npos)
    {
      pos = _ctx.text.size() - 1;

      break;
    }
  }

  auto const end = pos - start + 1;
  return _ctx.text.substr(start, end > max ? max : end);
}

void Fltrdr::set_focus_point()
{
  std::size_t begin {0};
  std::size_t end {_ctx.word.size()};

  // check for punct at beginning of word
  for (auto i = _ctx.word.begin(); i != _ctx.word.end(); ++i)
  {
    if (std::isalpha(static_cast<unsigned char>(*i)) != 0)
    {
      break;
    }

    ++begin;
  }

  // check for punct at end of word
  for (auto i = _ctx.word.rbegin(); i != _ctx.word.rend(); ++i)
  {
    if (std::isalpha(static_cast<unsigned char>(*i)) != 0)
    {
      if (*i == 's' && ++i != _ctx.word.rend() && *i == '\'')
      {
        end -= 2;
      }

      break;
    }

    --end;
  }

  auto size {end - begin};
  if (size > _ctx.word.size() || size == 0)
  {
    size = _ctx.word.size();
  }

  if (size < 13)
  {
    _ctx.focus_point = std::round(size * _ctx.focus);
  }
  else
  {
    _ctx.focus_point = 3;
  }

  if (_ctx.word.size() != size)
  {
    _ctx.focus_point += begin;
  }
}

void Fltrdr::set_line(std::size_t offset)
{
  _ctx.line = {};
  current_word();
  set_focus_point();

  if (_ctx.show_line)
  {
    _ctx.line.prev = buf_prev(offset);
    _ctx.line.next = buf_next(offset);
  }
  else
  {
    if (_ctx.show_prev)
    {
      _ctx.line.prev = buf_prev(offset);
    }

    if (_ctx.show_next)
    {
      _ctx.line.next = buf_next(offset);
    }
  }

  auto const width_left = (_ctx.width / 2) - 1 - offset;
  auto const width_right = (_ctx.width / 2) + 1 + offset;

  std::size_t pad_left {width_left - _ctx.focus_point - _ctx.line.prev.size()};
  std::size_t pad_right {width_right - _ctx.word.size() + _ctx.focus_point - _ctx.line.next.size()};

  if (_ctx.width % 2 != 0)
  {
    ++pad_right;
  }

  _ctx.line.prev = OB::String::repeat(pad_left, aec::space) + _ctx.line.prev;
  _ctx.line.curr += _ctx.word;
  _ctx.line.next +=  OB::String::repeat(pad_right, aec::space);
}

Fltrdr::Line Fltrdr::get_line()
{
  return _ctx.line;
}

void Fltrdr::prev_sentence()
{
  if (_ctx.index == _ctx.index_min)
  {
    return;
  }

  prev_word();
  if (_ctx.word.find_first_of(".!?") != std::string::npos)
  {
    prev_word();
    if (_ctx.word.find_first_of(".!?") != std::string::npos)
    {
      next_word();
      return;
    }
  }
  while (_ctx.index > _ctx.index_min)
  {
    prev_word();
    if (_ctx.word.find_first_of(".!?") != std::string::npos)
    {
      next_word();
      break;
    }
  }
}

void Fltrdr::next_sentence()
{
  if (_ctx.index == _ctx.index_max)
  {
    return;
  }

  while (_ctx.index < _ctx.index_max)
  {
    if (_ctx.word.find_first_of(".!?") != std::string::npos)
    {
      next_word();
      break;
    }
    next_word();
  }
}

void Fltrdr::prev_chapter()
{
  if (_ctx.index == _ctx.index_min)
  {
    return;
  }

  while (_ctx.index > _ctx.index_min)
  {
    prev_word();
    if (OB::String::lowercase(_ctx.word).find("chapter") != std::string::npos)
    {
      break;
    }
  }
}

void Fltrdr::next_chapter()
{
  if (_ctx.index == _ctx.index_max)
  {
    return;
  }

  while (_ctx.index < _ctx.index_max)
  {
    next_word();
    if (OB::String::lowercase(_ctx.word).find("chapter") != std::string::npos)
    {
      break;
    }
  }
}

std::string Fltrdr::word()
{
  return _ctx.word;
}

void Fltrdr::current_word()
{
  auto const pos = _ctx.text.find(" ", _ctx.pos + 1);
  if (pos != std::string::npos)
  {
    _ctx.word = _ctx.text.substr(_ctx.pos + 1, pos - _ctx.pos - 1);
  }
  else
  {
    _ctx.word = _ctx.text.substr(_ctx.pos + 1);
  }
}

bool Fltrdr::prev_word()
{
  if (_ctx.index > _ctx.index_min)
  {
    --_ctx.index;

    auto const pos = _ctx.text.rfind(" ", _ctx.pos - 1);
    if (pos != std::string::npos)
    {
      _ctx.pos = pos;
      current_word();

      return true;
    }
  }

  return false;
}

bool Fltrdr::next_word()
{
  if (_ctx.index < _ctx.index_max)
  {
    ++_ctx.index;

    auto const pos = _ctx.text.find(" ", _ctx.pos + 1);
    if (pos != std::string::npos && _ctx.text.at(pos))
    {
      _ctx.pos = pos;
      current_word();

      return true;
    }
  }

  return false;
}

void Fltrdr::set_index(std::size_t i)
{
  if (i <= _ctx.index_min)
  {
    i = _ctx.index_min;
  }
  else if (i >= _ctx.index_max)
  {
    i = _ctx.index_max;
  }

  if (i < _ctx.index)
  {
    while (i != _ctx.index)
    {
      prev_word();
    }
  }
  else if (i > _ctx.index)
  {
    while (i != _ctx.index)
    {
      next_word();
    }
  }
}

std::size_t Fltrdr::get_index()
{
  return _ctx.index;
}

int Fltrdr::get_wait()
{
  bool punc {false};

  // check for punct at end of word
  for (auto i = _ctx.word.rbegin(); i != _ctx.word.rend(); ++i)
  {
    if (std::isalpha(static_cast<unsigned char>(*i)) == 0)
    {
      switch (*i)
      {
        case ',': case '.': case ';':
        case ':': case '?': case '!':
        case '\"': case '-': case ')':
          punc = true;
          break;
        default:
          break;
      }
    }
    else
    {
      break;
    }
  }

  int ms {60000 / _ctx.wpm};
  int wait_std {static_cast<int>(ms * (1 + (_ctx.word.size() / 100 * 4.0)))};
  int wait_lng {wait_std * 2};

  if (_ctx.slow)
  {
    wait_std = static_cast<int>(wait_std * 1.10);
    wait_lng = static_cast<int>(wait_lng * 1.10);
  }

  if (punc)
  {
    ms = wait_lng;
  }
  else
  {
    ms = wait_std;
  }

  // set ms
  _ctx.ms = ms;

  return _ctx.ms;
}

void Fltrdr::calc_wpm_avg()
{
  // calc average wpm
  _ctx.wpm_total += (60000 / _ctx.ms);
  _ctx.wpm_avg = _ctx.wpm_total / ++_ctx.wpm_count;
}

int Fltrdr::get_wpm()
{
  return _ctx.wpm;
}

void Fltrdr::set_wpm(int const i)
{
  _ctx.wpm = i;

  if (_ctx.wpm > _ctx.wpm_max)
  {
    _ctx.wpm = _ctx.wpm_max;
  }
  else if (_ctx.wpm < _ctx.wpm_min)
  {
    _ctx.wpm = _ctx.wpm_min;
  }
}

void Fltrdr::inc_wpm()
{
  _ctx.wpm += _ctx.wpm_diff;

  if (_ctx.wpm > _ctx.wpm_max)
  {
    _ctx.wpm = _ctx.wpm_max;
  }
}

void Fltrdr::dec_wpm()
{
  _ctx.wpm -= _ctx.wpm_diff;

  if (_ctx.wpm < _ctx.wpm_min)
  {
    _ctx.wpm = _ctx.wpm_min;
  }
}

std::string Fltrdr::get_stats()
{
  std::ostringstream buf;

  buf
  << timer.str() << " "
  << _ctx.wpm_avg << "avg "
  << _ctx.wpm << "wpm "
  << _ctx.index << "w "
  << static_cast<int>(_ctx.index / static_cast<double>(_ctx.index_max) * 100) << "%";

  return buf.str();
}

void Fltrdr::set_show_line(bool const val)
{
  _ctx.show_line = val;
}

bool Fltrdr::get_show_line()
{
  return _ctx.show_line;
}

void Fltrdr::set_show_prev(int const val)
{
  if (val >= _ctx.show_min && val <= _ctx.show_max)
  {
    _ctx.show_prev = val;
  }
}

int Fltrdr::get_show_prev()
{
  return _ctx.show_prev;
}

void Fltrdr::set_show_next(int const val)
{
  if (val >= _ctx.show_min && val <= _ctx.show_max)
  {
    _ctx.show_next = val;
  }
}

int Fltrdr::get_show_next()
{
  return _ctx.show_next;
}

std::size_t Fltrdr::progress()
{
  return static_cast<std::size_t>(_ctx.index / static_cast<double>(_ctx.index_max) * 100);
}

bool Fltrdr::search_next()
{
  auto const end = std::sregex_iterator();

  if (_ctx.search.it == end)
  {
    return false;
  }

  if (_ctx.search.forward)
  {
    bool last {false};
    auto const index = get_index();
    next_word();

    for (auto ptr = _ctx.search.it; ptr != end; ++ptr)
    {
      if (static_cast<std::size_t>(ptr->position()) > _ctx.pos)
      {
        auto pos = static_cast<std::size_t>(ptr->position());

        while (_ctx.pos < pos)
        {
          if (! next_word())
          {
            last = true;
            break;
          }
        }

        break;
      }
    }

    auto const index_new = get_index();
    if (index != _ctx.index_max)
    {
      if (! last || (index_new != index && index_new != _ctx.index_max))
      {
        prev_word();
      }
    }
  }
  else
  {
    for (auto ptr = _ctx.search.it, prev = _ctx.search.it; prev != end; ++ptr)
    {
      if (ptr == end || static_cast<std::size_t>(ptr->position()) > _ctx.pos)
      {
        auto pos = static_cast<std::size_t>(prev->position());

        while (_ctx.pos > pos)
        {
          if (! prev_word())
          {
            break;
          }
        }

        break;
      }

      prev = ptr;
    }
  }

  return true;
}

bool Fltrdr::search_prev()
{
  auto const end = std::sregex_iterator();

  if (_ctx.search.it == end)
  {
    return false;
  }

  if (_ctx.search.forward)
  {
    for (auto ptr = _ctx.search.it, prev = _ctx.search.it; prev != end; ++ptr)
    {
      if (ptr == end || static_cast<std::size_t>(ptr->position()) > _ctx.pos)
      {
        auto pos = static_cast<std::size_t>(prev->position());

        while (_ctx.pos > pos)
        {
          if (! prev_word())
          {
            break;
          }
        }

        break;
      }

      prev = ptr;
    }
  }
  else
  {
    bool last {false};
    auto const index = get_index();
    next_word();

    for (auto ptr = _ctx.search.it; ptr != end; ++ptr)
    {
      if (static_cast<std::size_t>(ptr->position()) > _ctx.pos)
      {
        auto pos = static_cast<std::size_t>(ptr->position());

        while (_ctx.pos < pos)
        {
          if (! next_word())
          {
            last = true;
            break;
          }
        }

        break;
      }
    }

    auto const index_new = get_index();
    if (index != _ctx.index_max)
    {
      if (! last || (index_new != index && index_new != _ctx.index_max))
      {
        prev_word();
      }
    }
  }

  return true;
}

bool Fltrdr::search_forward(std::string const& rx)
{
  _ctx.search.forward = true;
  _ctx.search.begin = _ctx.text.begin();
  _ctx.search.end = _ctx.text.end();

  try
  {
    _ctx.search.rgx = std::regex(rx, std::regex::optimize | std::regex::icase);
    _ctx.search.it = std::sregex_iterator(
      _ctx.search.begin, _ctx.search.end, _ctx.search.rgx,
      std::regex_constants::match_not_null);
  }
  catch (...)
  {
    _ctx.search.rgx = {};
    _ctx.search.it = std::sregex_iterator();
    return false;
  }

  return search_next();
}

bool Fltrdr::search_backward(std::string const& rx)
{
  _ctx.search.forward = false;
  _ctx.search.begin = _ctx.text.begin();
  _ctx.search.end = _ctx.text.end();

  try
  {
    _ctx.search.rgx = std::regex(rx, std::regex::optimize | std::regex::icase);
    _ctx.search.it = std::sregex_iterator(
      _ctx.search.begin, _ctx.search.end, _ctx.search.rgx,
      std::regex_constants::match_not_null);
  }
  catch (...)
  {
    _ctx.search.rgx = {};
    _ctx.search.it = std::sregex_iterator();
    return false;
  }

  return search_next();
}

void Fltrdr::reset_timer()
{
  timer.reset();
}

void Fltrdr::reset_wpm_avg()
{
  _ctx.wpm_avg = 0;
  _ctx.wpm_count = 0;
  _ctx.wpm_total = 0;
}
