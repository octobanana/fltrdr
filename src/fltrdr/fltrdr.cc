#include "fltrdr/fltrdr.hh"

#include "ob/string.hh"
#include "ob/timer.hh"
#include "ob/text.hh"
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
#include <iterator>
#include <random>

using namespace std::string_literals;

void Fltrdr::init()
{
  _ctx.pos = 0;
  _ctx.index = 1;

  _ctx.text.clear();
  _ctx.text.shrink_to_fit();

  reset_timer();
  reset_wpm_avg();
}

bool Fltrdr::parse(std::istream& input)
{
  init();

  OB::Text::String word;
  std::size_t word_count {0};

  while (input >> std::ws >> word)
  {
    if (word.size() > 1 && word.cols() == word.size() * 2)
    {
      for (auto const& e : word)
      {
        _ctx.text.str() += " " + std::string(e.str);
        ++word_count;
      }
    }
    else
    {
      _ctx.text.str() += " " + word.str();
      ++word_count;
    }
  }

  if (word_count == 0)
  {
    _ctx.text.str() = " fltrdr";
    ++word_count;

    _ctx.text.sync();
    _ctx.index_max = word_count;

    return false;
  }

  _ctx.text.sync();
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

OB::Text::View Fltrdr::buf_prev(std::size_t offset)
{
  if (! _ctx.pos)
  {
    return {};
  }

  auto const width = (_ctx.width / 2) - 1 - offset;
  auto size = static_cast<int>(width - _ctx.prefix_width);

  if (size < 1)
  {
    return {};
  }

  auto pos = _ctx.pos + 1;
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
    pos = _ctx.text.rfind(" "s, static_cast<std::size_t>(--pos));

    if (pos <= min || pos == OB::Text::String::npos || pos == 0)
    {
      break;
    }
  }

  auto const start = pos < min ? min : pos;
  auto const len = _ctx.pos + 1 - start < static_cast<std::size_t>(size) ?
    _ctx.pos + 1 - start : static_cast<std::size_t>(size);

  return _ctx.text.substr(start, len);
}

OB::Text::View Fltrdr::buf_next(std::size_t offset)
{
  auto pos = _ctx.text.find(" "s, static_cast<std::size_t>(_ctx.pos + 1));
  auto const start = pos;

  if (pos == OB::Text::String::npos)
  {
    return {};
  }

  auto const width = (_ctx.width / 2) + 1 + offset;
  auto size = static_cast<int>(width - (_ctx.word.size() - _ctx.prefix_width));

  if (size < 1)
  {
    return {};
  }

  if (_ctx.width % 2 != 0)
  {
    ++size;
  }

  auto p = pos;
  auto count = size;

  while (++count != size && ++p != _ctx.text.size() - 1);

  auto const max = static_cast<std::size_t>(size);

  if (_ctx.show_line)
  {
    return _ctx.text.substr(pos, max);
  }

  for (int i = 0; i < _ctx.show_next; ++i)
  {
    pos = _ctx.text.find(" "s, ++pos);

    if (pos - start + 1 >= max || pos == OB::Text::String::npos)
    {
      pos = _ctx.text.size() - 1;

      break;
    }
  }

  auto const end = pos - start + 1;

  return _ctx.text.substr(start, end < max ? end : max);
}

void Fltrdr::set_focus_point()
{
  std::size_t begin {0};
  std::size_t end {_ctx.word.size()};

  // check for punct at beginning of word
  for (auto i = _ctx.word.begin(); i != _ctx.word.end(); ++i)
  {
    if (! OB::Text::is_punct(OB::Text::to_int32(i->str)))
    {
      break;
    }

    ++begin;
  }

  // check for punct at end of word
  for (auto i = _ctx.word.rbegin(); i != _ctx.word.rend(); ++i)
  {
    if (! OB::Text::is_punct(OB::Text::to_int32(i->str)))
    {
      if (i->str == "s" && ++i != _ctx.word.rend() &&
        OB::Text::is_punct(OB::Text::to_int32(i->str)))
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

  // calc display columns needed up to focus point position
  _ctx.prefix_width = _ctx.word.at(_ctx.focus_point).tcols;
}

void Fltrdr::set_line(std::size_t offset)
{
  _ctx.line = {};
  _ctx.prev.clear();
  _ctx.next.clear();

  current_word();
  set_focus_point();

  if (_ctx.show_line)
  {
    _ctx.prev = buf_prev(offset);
    _ctx.next = buf_next(offset);
  }
  else
  {
    if (_ctx.show_prev)
    {
      _ctx.prev = buf_prev(offset);
    }

    if (_ctx.show_next)
    {
      _ctx.next = buf_next(offset);
    }
  }

  auto const width_left = (_ctx.width / 2) - 1 - offset;
  auto const width_right = (_ctx.width / 2) + 1 + offset;

  auto pad_left {static_cast<int>(width_left - _ctx.prefix_width - _ctx.prev.size())};
  auto pad_right {static_cast<int>(width_right - _ctx.word.size() + _ctx.prefix_width - _ctx.next.size())};

  if (_ctx.width % 2 != 0)
  {
    ++pad_right;
  }

  if (pad_left < 0)
  {
    pad_left = 0;
  }

  if (pad_right < 0)
  {
    pad_right = 0;
  }


  _ctx.line.prev = OB::String::repeat(static_cast<std::size_t>(pad_left), aec::space) + std::string(_ctx.prev.str());

  _ctx.line.curr = _ctx.word;

  _ctx.line.next += std::string(_ctx.next.str()) + OB::String::repeat(static_cast<std::size_t>(pad_right), aec::space);
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
  if (_ctx.word.find_first_of(_ctx.sentence_end) != OB::Text::String::npos)
  {
    prev_word();
    if (_ctx.word.find_first_of(_ctx.sentence_end) != OB::Text::String::npos)
    {
      next_word();
      return;
    }
  }
  while (_ctx.index > _ctx.index_min)
  {
    prev_word();
    if (_ctx.word.find_first_of(_ctx.sentence_end) != OB::Text::String::npos)
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
    if (_ctx.word.find_first_of(_ctx.sentence_end) != OB::Text::String::npos)
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
    if (_ctx.word.str() == "chapter")
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
    if (_ctx.word.str() == "chapter")
    {
      break;
    }
  }
}

OB::Text::View Fltrdr::word()
{
  return _ctx.word;
}

void Fltrdr::current_word()
{
  auto const pos = _ctx.text.find(" "s, _ctx.pos + 1);
  if (pos != OB::Text::String::npos)
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

    auto const pos = _ctx.text.rfind(" "s, _ctx.pos - 1);
    if (pos != OB::Text::String::npos)
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

    auto const pos = _ctx.text.find(" "s, _ctx.pos + 1);
    if (pos != OB::Text::String::npos)
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
    if (OB::Text::is_punct(OB::Text::to_int32(i->str)))
    {
      switch (OB::Text::to_int32(i->str))
      {
        case U',': case U'.': case U';':
        case U':': case U'?': case U'!':
        case U'…':
          punc = true;
          break;
        default:
          break;
      }

      if (punc)
      {
        break;
      }
    }
    else
    {
      break;
    }
  }

  auto const wait_std = static_cast<int>((60000 / _ctx.wpm) * (1 + (_ctx.word.size() / 100 * 4.0)));

  // set ms
  if (punc)
  {
    _ctx.ms = wait_std * 2;
  }
  else
  {
    _ctx.ms = wait_std;
  }

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

bool Fltrdr::search_forward()
{
  if (_ctx.search.it.empty())
  {
    return false;
  }

  bool last {false};
  auto const index = get_index();
  next_word();

  for (auto const& e : _ctx.search.it)
  {
    auto const pos = _ctx.text.byte_to_char(e.pos);

    if (_ctx.pos < pos)
    {
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

  return true;
}

bool Fltrdr::search_backward()
{
  if (_ctx.search.it.empty())
  {
    return false;
  }

  std::size_t end {_ctx.search.it.size()};

  for (std::size_t i = 0, j = 0; j < end; j = i, ++i)
  {
    if (i == end || _ctx.text.byte_to_char(_ctx.search.it.at(i).pos) > _ctx.pos)
    {
      auto const pos = _ctx.text.byte_to_char(_ctx.search.it.at(j).pos);

      while (_ctx.pos > pos)
      {
        if (! prev_word())
        {
          break;
        }
      }

      break;
    }
  }

  return true;
}

bool Fltrdr::search_next()
{
  return _ctx.search.forward ? search_forward() : search_backward();
}

bool Fltrdr::search_prev()
{
  return _ctx.search.forward ? search_backward() : search_forward();
}

bool Fltrdr::search(std::string const& rx, bool forward)
{
  try
  {
    _ctx.search.forward = forward;
    _ctx.search.rx = rx;
    _ctx.search.it.match(_ctx.search.rx, _ctx.text.str());

    return search_next();
  }
  catch (...)
  {
    _ctx.search.it.clear();

    return false;
  }
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
