#include "fltrdr/tui.hh"

#include "ob/algorithm.hh"
#include "ob/string.hh"
#include "ob/text.hh"
#include "ob/term.hh"
namespace aec = OB::Term::ANSI_Escape_Codes;

#include <ctime>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <regex>
#include <utility>
#include <optional>
#include <limits>

#include <filesystem>
namespace fs = std::filesystem;

Tui::Tui() :
  _colorterm {OB::Term::is_colorterm()}
{
}

Tui& Tui::init(fs::path const& path)
{
  _ctx.file.path.clear();
  _ctx.file.name.clear();

  // parse from string
  if (path.empty())
  {
    std::stringstream ss; ss
    << "fltrdr";

    _fltrdr.parse(ss);
  }

  // parse from stdin
  else if (path == "*stdin*")
  {
    if (_fltrdr.parse(std::cin))
    {
      _ctx.file.path = "*stdin*";
      _ctx.file.name = "*stdin*";
    }
  }

  // parse from file
  else
  {
    if (! fs::exists(path))
    {
      throw std::runtime_error("the file does not exist '" + path.string() + "'");
    }

    std::ifstream ifile {path};
    if (! ifile.is_open())
    {
      throw std::runtime_error("could not open the file '" + path.string() + "'");
    }

    if (_fltrdr.parse(ifile))
    {
      _ctx.file.path = path;
      _ctx.file.name = path.lexically_normal().string();
    }
  }

  return *this;
}

bool Tui::press_to_continue(std::string const& str, char32_t val)
{
  std::cerr
  << "Press " << str << " to continue";

  _term_mode.set_min(1);
  _term_mode.set_raw();

  bool res {false};
  char32_t key {0};
  if ((key = OB::Term::get_key()) > 0)
  {
    res = (val == 0 ? true : val == key);
  }

  _term_mode.set_cooked();

  std::cerr
  << aec::nl;

  return res;
}

void Tui::base_config(fs::path const& path)
{
  _ctx.base_config = path;
}

void Tui::load_config(fs::path const& path)
{
  // ignore config if path equals "NONE"
  if (path == "NONE")
  {
    return;
  }

  // buffer for error output
  std::ostringstream err;

  if (! path.empty() && fs::exists(path))
  {
    std::ifstream file {path};

    if (file.is_open())
    {
      std::string line;
      std::size_t lnum {0};

      while (std::getline(file, line))
      {
        // increase line number
        ++lnum;

        // trim leading and trailing whitespace
        line = OB::String::trim(line);

        // ignore empty line or comment
        if (line.empty() || OB::String::assert_rx(line, std::regex("^#[^\\r]*$")))
        {
          continue;
        }

        if (auto const res = command(line))
        {
          if (! res.value().first)
          {
            // source:line: level: info
            err << path.string() << ":" << lnum << ": " << res.value().second << "\n";
          }
        }
      }
    }
    else
    {
      err << "error: could not open config file '" << path.string() << "'\n";
    }
  }

  if (! err.str().empty())
  {
    std::cerr << err.str();

    if (! press_to_continue("ENTER", '\n'))
    {
      throw std::runtime_error("aborted by user");
    }
  }
}

bool Tui::save_state()
{
  if (_ctx.base_config.empty())
  {
    set_status(false, "empty base config directory");

    return false;
  }

  auto const content_id = _fltrdr.content_id();

  if (content_id.empty())
  {
    set_status(false, "empty content id");

    return false;
  }

  fs::path path {_ctx.base_config / fs::path("state") / fs::path(_fltrdr.content_id())};

  std::ofstream file {path, std::ios::trunc};

  if (! file.is_open())
  {
    set_status(false, "could not open file");

    return false;
  }

  // timestamp
  std::time_t t = std::time(0);
  std::tm tm = *std::localtime(&t);

  // dump current state to file
  file
  << "# fltrdr state\n"
  << "# file: " << _ctx.file.path.string() << "\n"
  << "# date: " << std::put_time(&tm, "%FT%TZ\n")
  << "\n"
  << "goto " << _fltrdr.get_index() << "\n"
  << "wpm " << _fltrdr.get_wpm() << "\n"
  << "wpm-avg " << _fltrdr.get_wpm_avg() << "\n"
  << "timer " << _fltrdr.timer.str() << "\n"
  << std::flush;

  set_status(true, "saved state");

  return true;
}

bool Tui::load_state()
{
  if (_ctx.base_config.empty())
  {
    return false;
  }

  auto const content_id = _fltrdr.content_id();

  if (content_id.empty())
  {
    return false;
  }

  fs::path path {_ctx.base_config / fs::path("state") / fs::path(_fltrdr.content_id())};

  if (! fs::exists(path))
  {
    return false;
  }

  std::ifstream file {path};

  if (file.is_open())
  {
    std::string line;
    std::size_t lnum {0};

    while (std::getline(file, line))
    {
      // ignore empty line or comment
      if (line.empty() || OB::String::assert_rx(line, std::regex("^#[^\\r]*$")))
      {
        continue;
      }

      command(line);
    }
  }

  return true;
}

void Tui::load_hist_command(fs::path const& path)
{
  _readline.hist_load(path);
}

void Tui::load_hist_search(fs::path const& path)
{
  _readline_search.hist_load(path);
}

void Tui::run()
{
  std::cout
  << aec::cursor_hide
  << aec::screen_push
  << aec::cursor_hide
  << aec::screen_clear
  << aec::cursor_home
  << aec::mouse_enable
  << std::flush;

  // set terminal mode to raw
  _term_mode.set_min(0);
  _term_mode.set_raw();

  // start the event loop
  event_loop();

  std::cout
  << aec::mouse_disable
  << aec::nl
  << aec::screen_pop
  << aec::cursor_show
  << std::flush;
}

void Tui::event_loop()
{
  while (_ctx.is_running)
  {
    // get the terminal width and height
    OB::Term::size(_ctx.width, _ctx.height);

    // check for correct screen size
    if (screen_size() != 0)
    {
      pause();
      std::this_thread::sleep_for(std::chrono::milliseconds(_ctx.input_interval));

      char32_t key {0};
      if ((key = OB::Term::get_key()) > 0)
      {
        switch (key)
        {
          case 'q': case 'Q':
          case OB::Term::ctrl_key('c'):
          {
            _ctx.is_running = false;

            break;
          }

          default:
          {
            break;
          }
        }
      }

      continue;
    }

    // update screen size
    _fltrdr.screen_size(_ctx.width, _ctx.height);

    // update offset
    _ctx.offset = static_cast<std::size_t>(_ctx.offset_value / 10.0 * static_cast<double>(_ctx.width / 2));

    // play
    if (_ctx.state.play && ! _ctx.state.counting_down)
    {
      // move to next word
      _fltrdr.next_word();

      // calculate new wpm average
      _fltrdr.calc_wpm_avg();

      // check for end of file
      if (_fltrdr.eof())
      {
        pause();
      }
    }

    // render new content
    _fltrdr.set_line(_ctx.offset);
    clear();
    draw();
    refresh();

    if (_ctx.state.counting_down)
    {
      if (_ctx.state.count_down == 0)
      {
        _ctx.state.counting_down = false;
        _fltrdr.timer.start();
      }
      else
      {
        --_ctx.state.count_down;
      }
    }

    set_wait();
    auto wait = _ctx.state.wait;

    while (_ctx.is_running && wait)
    {
      if (wait > _ctx.input_interval)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(_ctx.input_interval));
        wait -= _ctx.input_interval;
      }
      else
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(wait));
        wait = 0;
      }

      get_input(wait);
    }
  }
}

void Tui::clear()
{
  // clear screen
  _ctx.buf
  << aec::cursor_home
  << _ctx.style.bg;

  OB::Algorithm::for_each(_ctx.height,
  [&](auto) {
    OB::Algorithm::for_each(_ctx.width, [&](auto) {
      _ctx.buf << " "; });
    _ctx.buf << "\n";
  },
  [&](auto) {
    OB::Algorithm::for_each(_ctx.width, [&](auto) {
      _ctx.buf << " "; });
  });

  _ctx.buf
  << aec::clear
  << aec::cursor_home;
}

void Tui::refresh()
{
  // output buffer to screen
  std::cout
  << _ctx.buf.str()
  << std::flush;

  // clear output buffer
  _ctx.buf.str("");
}

void Tui::draw()
{
  draw_content();
  draw_border_top();
  draw_border_bottom();
  draw_progress_bar();
  draw_status();
  draw_prompt_message();
  draw_keybuf();
}

void Tui::draw_content()
{
  _ctx.buf
  << aec::cursor_save
  << aec::cursor_set(0, (_ctx.height / 2) - 1)
  << aec::erase_line;

  struct Block
  {
    std::string before {};
    std::string value {};
    std::string after {aec::clear};
  };
  using Buf = std::vector<Block>;
  Buf buf {_ctx.width, Block()};

  // get args for building the line
  auto const line = _fltrdr.get_line();

  // get text views for each line
  OB::Text::View prev {line.prev};
  OB::Text::View curr {line.curr};
  OB::Text::View next {line.next};

  std::size_t width_left {(_ctx.width / 2) - _ctx.offset};
  std::size_t width_right {(_ctx.width / 2) + _ctx.offset + (_ctx.width % 2 != 0 ? 1 : 0)};

  auto perc_left = static_cast<std::size_t>(static_cast<double>(width_left) * (_ctx.state.count_down / static_cast<double>(_ctx.state.count_total)));
  auto perc_right = static_cast<std::size_t>(static_cast<double>(width_right) * (_ctx.state.count_down / static_cast<double>(_ctx.state.count_total)));

  auto pad_left = width_left - perc_left;
  auto pad_right = width_right - perc_right;

  // add background style if counting down
  if (_ctx.state.counting_down)
  {
    if (_ctx.state.count_down)
    {
      auto const end = pad_left + perc_left + perc_right;

      for (auto i = pad_left; i < end; ++i)
      {
        buf.at(i).before += _ctx.style.countdown;
      }
    }
    else
    {
      buf.at(width_left - 1).before += _ctx.style.countdown;
    }
  }

  // number of display columns used
  std::size_t cols {0};

  // add line prev to buf
  // iterate in reverse in case display columns exceed buf size
  std::size_t const npos {std::numeric_limits<std::size_t>::max()};
  for (std::size_t i = prev.size() - 1; i != npos; --i)
  {
    auto const& val = prev.at(i);

    cols += val.cols;
    if (cols > prev.size())
    {
      cols -= val.cols;

      // add padding
      buf.at(i).value = OB::String::repeat(prev.size() - cols, " ");
      cols = prev.size();

      break;
    }

    buf.at(i).value = val.str;

    if (buf.at(i).value == " ")
    {
      continue;
    }

    if (buf.at(i).value == "'" || buf.at(i).value == "\"")
    {
      buf.at(i).before += _ctx.style.word_quote;
    }
    else if (OB::Text::is_punct(OB::Text::to_int32(buf.at(i).value)))
    {
      buf.at(i).before += _ctx.style.word_punct;
    }
    else
    {
      buf.at(i).before += _ctx.style.word_secondary;
    }
  }

  // add line curr to buf
  bool highlight {false};
  for (std::size_t i = 0; i < curr.size(); ++i)
  {
    auto const val = curr.at(i);

    cols += val.cols;
    if (cols > buf.size())
    {
      cols -= val.cols;

      break;
    }

    auto const it = i + prev.size();
    if (it >= buf.size())
    {
      break;
    }

    buf.at(it).value = val.str;

    if (! highlight && cols >= width_left)
    {
      highlight = true;
      buf.at(it).before += _ctx.style.word_highlight;
    }
    else if (buf.at(it).value == "'" || buf.at(it).value == "\"")
    {
      buf.at(it).before += _ctx.style.word_quote;
    }
    else if (OB::Text::is_punct(OB::Text::to_int32(buf.at(it).value)))
    {
      buf.at(it).before += _ctx.style.word_punct;
    }
    else
    {
      buf.at(it).before += _ctx.style.word_primary;
    }
  }

  // add line next to buf
  for (std::size_t i = 0; i < next.size(); ++i)
  {
    auto const val = next.at(i);

    auto const it = i + prev.size() + curr.size();

    cols += val.cols;
    if (cols > buf.size())
    {
      cols -= val.cols;

      // add padding
      buf.at(it).value = OB::String::repeat(buf.size() - cols, " ");
      cols = buf.size();

      break;

    }

    buf.at(it).value = val.str;

    if (buf.at(it).value == " ")
    {
      continue;
    }

    if (buf.at(it).value == "'" || buf.at(it).value == "\"")
    {
      buf.at(it).before += _ctx.style.word_quote;
    }
    else if (OB::Text::is_punct(OB::Text::to_int32(buf.at(it).value)))
    {
      buf.at(it).before += _ctx.style.word_punct;
    }
    else
    {
      buf.at(it).before += _ctx.style.word_secondary;
    }
  }

  // render line to buffer
  for (auto const& e : buf)
  {
    _ctx.buf
    << _ctx.style.bg
    << e.before
    << e.value
    << e.after;
  }

  _ctx.buf
  << aec::clear
  << aec::cursor_load;
}

void Tui::draw_keybuf()
{
  if (_ctx.keys.empty())
  {
    return;
  }

  _ctx.buf
  << aec::cursor_save
  << aec::cursor_set(_ctx.width - 3, _ctx.height)
  << _ctx.style.bg
  << "    "
  << aec::cursor_set(_ctx.width - 3, _ctx.height)
  << _ctx.style.secondary
  << aec::space;

  for (auto const& e : _ctx.keys)
  {
    if (OB::Text::is_print(static_cast<std::int32_t>(e.val)))
    {
      _ctx.buf
      << e.str;
    }
  }

  _ctx.buf
  << aec::space
  << aec::clear
  << aec::cursor_load;
}

void Tui::draw_progress_bar()
{
  if (! _ctx.show.progress)
  {
    return;
  }

  auto height = _ctx.height - 2;
  if (! _ctx.show.status)
  {
    height = _ctx.height - 1;
  }

  _ctx.buf
  << aec::cursor_save
  << aec::cursor_set(0, height)
  << aec::erase_line
  << _ctx.style.bg
  << _ctx.style.progress_bar
  << OB::String::repeat(_ctx.width, _ctx.sym.progress_bar)
  << aec::clear
  << aec::cr
  << _ctx.style.bg
  << _ctx.style.progress_fill
  << OB::String::repeat((_fltrdr.progress() * _ctx.width) / 100, _ctx.sym.progress_fill)
  << aec::clear
  << aec::cursor_load;
}

void Tui::draw_prompt_message()
{
  // check if command prompt message is active
  if (_ctx.prompt.count > 0)
  {
    --_ctx.prompt.count;

    _ctx.buf
    << aec::cursor_save
    << aec::cursor_set(0, _ctx.height)
    << _ctx.style.bg
    << _ctx.style.prompt
    << ">"
    << _ctx.style.prompt_status
    << _ctx.prompt.str.substr(0, _ctx.width - 5)
    << aec::cursor_load;
  }
}

void Tui::draw_status()
{
  if (! _ctx.show.status)
  {
    return;
  }

  _ctx.buf
  << aec::cursor_save
  << aec::cursor_set(0, _ctx.height - 1);

  // mode
  _ctx.buf
  << _ctx.style.background
  << _ctx.style.primary
  << aec::space
  << _ctx.status.mode
  << aec::space
  << aec::clear
  << _ctx.style.bg
  << aec::space;
  int const len_mode {2 + static_cast<int>(_ctx.status.mode.size())};

  // file
  int len_file {2 + static_cast<int>(_ctx.file.name.size())};

  // stats
  std::string stats {_fltrdr.get_stats()};
  int const len_stats {2 + static_cast<int>(stats.size())};

  // pad center
  int pad_center {static_cast<int>(_ctx.width) - len_mode - len_file - len_stats};
  int len_center {pad_center};

  if (pad_center >= 0)
  {
    _ctx.buf
    << _ctx.style.bg
    << _ctx.style.secondary
    << _ctx.file.name
    << aec::space;

    while (pad_center--)
    {
      _ctx.buf
      << aec::space;
    }

    _ctx.buf
    << aec::clear
    << _ctx.style.background
    << _ctx.style.primary
    << aec::space
    << stats
    << aec::space
    << aec::clear;
  }
  else
  {
    if (static_cast<std::size_t>(std::abs(len_center)) < (_ctx.file.name.size()))
    {
      _ctx.buf
      << _ctx.style.secondary
      << "<"
      << _ctx.file.name.substr(static_cast<std::size_t>(std::abs(len_center)) + 1)
      << aec::space
      << aec::clear
      << _ctx.style.background
      << _ctx.style.primary
      << aec::space
      << stats
      << aec::space
      << aec::clear;
    }
    else if (static_cast<std::size_t>(std::abs(len_center)) == (_ctx.file.name.size()))
    {
      _ctx.buf
      << aec::space
      << _ctx.style.background
      << _ctx.style.primary
      << aec::space
      << stats
      << aec::space
      << aec::clear;
    }
    else if (static_cast<std::size_t>(std::abs(len_center)) == (_ctx.file.name.size() + 1))
    {
      _ctx.buf
      << _ctx.style.background
      << _ctx.style.primary
      << aec::space
      << stats
      << aec::space
      << aec::clear;
    }
    else
    {
      _ctx.buf
      << _ctx.style.background
      << _ctx.style.primary
      << aec::space
      << "<"
      << stats.substr(static_cast<std::size_t>(std::abs(len_center)) - _ctx.file.name.size())
      << aec::space
      << aec::clear;
    }
  }

  _ctx.buf << aec::cursor_load;
}

void Tui::draw_border_top()
{
  if (! _ctx.show.border_top)
  {
    return;
  }

  auto const width = (_ctx.width / 2) - _ctx.offset;
  auto const height = (_ctx.height / 2) - 2;

  _ctx.buf
  << aec::cursor_save
  << aec::cursor_set(0, height)
  << aec::erase_line
  << _ctx.style.bg
  << _ctx.style.border
  << OB::String::repeat(_ctx.width, _ctx.sym.border_top)
  << aec::cursor_set(width, height)
  << _ctx.sym.border_top_mark
  << aec::clear
  << aec::cursor_load;
}

void Tui::draw_border_bottom()
{
  if (! _ctx.show.border_bottom)
  {
    return;
  }

  auto const width = (_ctx.width / 2) - _ctx.offset;
  auto const height = (_ctx.height / 2);

  _ctx.buf
  << aec::cursor_save
  << aec::cursor_set(0, height)
  << aec::erase_line
  << _ctx.style.bg
  << _ctx.style.border
  << OB::String::repeat(_ctx.width, _ctx.sym.border_bottom)
  << aec::cursor_set(width, height)
  << _ctx.sym.border_bottom_mark
  << aec::clear
  << aec::cursor_load;
}

void Tui::set_wait()
{
  if (_ctx.state.play)
  {
    if (_ctx.state.counting_down)
    {
      _ctx.state.wait = (60000 / _fltrdr.get_wpm());
    }
    else
    {
      _ctx.state.wait = _fltrdr.get_wait();
    }
  }
  else
  {
    _ctx.state.wait = _ctx.state.refresh_rate;
  }
}

void Tui::set_status(bool success, std::string const& msg)
{
  _ctx.style.prompt_status = success ? _ctx.style.success : _ctx.style.error;
  _ctx.prompt.str = msg;
  _ctx.prompt.count = _ctx.prompt.timeout;
}

void Tui::get_input(int& wait)
{
  while ((_ctx.key.val = OB::Term::get_key(&_ctx.key.str)) > 0)
  {
    _ctx.keys.emplace_back(_ctx.key);

    switch (_ctx.keys.at(0).val)
    {
      // quit
      case 'q': case 'Q':
      {
        _ctx.is_running = false;
        _ctx.keys.clear();

        return;
      }

      // save state
      case 'w':
      {
        pause();
        save_state();

        break;
      }

      case OB::Term::ctrl_key('c'):
      {
        _ctx.is_running = false;
        _ctx.keys.clear();

        return;
      }

      case OB::Term::Key::escape:
      {
        // pause
        pause();
        _ctx.prompt.count = 0;
        _ctx.keys.clear();

        break;
      }

      // goto beginning
      case 'g':
      {
        if (_ctx.keys.size() < 2)
        {
          return;
        }
        else if (_ctx.keys.at(1).val == 'g')
        {
          pause();
          _fltrdr.begin();
        }

        break;
      }

      // goto end
      case 'G':
      {
        pause();
        _fltrdr.end();

        break;
      }

      // toggle play
      case OB::Term::Key::space:
      case OB::Term::Mouse::btn3_press:
      {
        _ctx.keys.clear();

        if (_ctx.state.play)
        {
          pause();
        }
        else
        {
          play();
          wait = 0;

          return;
        }

        break;
      }

      // increase show prev word by one
      case 'i':
      {
        _fltrdr.set_show_prev(_fltrdr.get_show_prev() + 1);

        break;
      }

      // decrease show prev word by one
      case 'I':
      {
        _fltrdr.set_show_prev(_fltrdr.get_show_prev() - 1);

        break;
      }

      // increase show next word by one
      case 'o':
      {
        _fltrdr.set_show_next(_fltrdr.get_show_next() + 1);

        break;
      }

      // decrease show next word by one
      case 'O':
      {
        _fltrdr.set_show_next(_fltrdr.get_show_next() - 1);

        break;
      }

      // search next current word
      case '*':
      {
        pause();
        _fltrdr.search(_fltrdr.word(), true);

        break;
      }

      // search prev current word
      case '#':
      {
        pause();
        _fltrdr.search(_fltrdr.word(), false);

        break;
      }

      // search next
      case 'n':
      {
        pause();
        _fltrdr.search_next();

        break;
      }

      // search prev
      case 'N':
      {
        pause();
        _fltrdr.search_prev();

        break;
      }

      // move index backwards
      case 'h': case OB::Term::Key::left:
      case OB::Term::Mouse::scroll_up:
      {
        pause();
        _fltrdr.prev_word();

        break;
      }

      // move index forwards
      case 'l': case OB::Term::Key::right:
      case OB::Term::Mouse::scroll_down:
      {
        pause();
        _fltrdr.next_word();

        break;
      }

      // move sentence backwards
      case 'H':
      {
        pause();
        _fltrdr.prev_sentence();

        break;
      }

      // move sentence forwards
      case 'L':
      {
        pause();
        _fltrdr.next_sentence();

        break;
      }

      // increase wpm
      case 'k': case OB::Term::Key::up:
      {
        _fltrdr.inc_wpm();

        break;
      }

      // decrease wpm
      case 'j': case OB::Term::Key::down:
      {
        _fltrdr.dec_wpm();

        break;
      }

      // move chapter backwards
      case 'J':
      {
        pause();
        _fltrdr.prev_chapter();

        break;
      }

      // move chapter forwards
      case 'K':
      {
        pause();
        _fltrdr.next_chapter();

        break;
      }

      // toggle extra words
      case 'v':
      case OB::Term::Mouse::btn2_press:
      {
        _fltrdr.set_show_line(! _fltrdr.get_show_line());

        break;
      }

      // command prompt
      case ':':
      {
        pause();
        command_prompt();
        _ctx.keys.clear();

        break;
      }

      // search forward
      case '/':
      {
        pause();
        search_forward();
        _ctx.keys.clear();

        break;
      }

      // search backward
      case '?':
      {
        pause();
        search_backward();
        _ctx.keys.clear();

        break;
      }

      default:
      {
        // ignore
        draw_keybuf();
        refresh();
        _ctx.keys.clear();

        return;
      }
    }

    // render new content
    _fltrdr.set_line(_ctx.offset);
    clear();
    draw();
    refresh();
    _ctx.keys.clear();
  }
}

void Tui::play()
{
  if (_ctx.state.play)
  {
    return;
  }

  _ctx.state.play = true;
  _ctx.status.mode = "PLAY";

  _ctx.state.counting_down = true;
  _ctx.state.count_down = _ctx.state.count_total;

  // reset prompt message count
  _ctx.prompt.count = 0;
}

void Tui::pause()
{
  if (! _ctx.state.play)
  {
    return;
  }

  if (_fltrdr.timer)
  {
    _fltrdr.timer.stop();
  }

  _ctx.state.play = false;
  _ctx.status.mode = "PAUSE";

  _ctx.state.counting_down = false;
  _ctx.state.count_down = 0;
}

std::optional<std::pair<bool, std::string>> Tui::command(std::string const& input)
{
  // quit
  if (! _ctx.is_running)
  {
    _ctx.is_running = false;
    return {};
  }

  // nop
  if (input.empty())
  {
    return {};
  }

  auto const keys = OB::String::split(input, " ");

  if (keys.empty())
  {
    return {};
  }

  // store the matches returned from OB::String::match
  std::optional<std::vector<std::string>> match_opt;

  // quit
  if (keys.size() == 1 && (keys.at(0) == "q" || keys.at(0) == "Q" ||
    keys.at(0) == "quit" || keys.at(0) == "Quit" || keys.at(0) == "exit"))
  {
    _ctx.is_running = false;
    return {};
  }

  // save state
  else if (keys.size() == 1 && keys.at(0) == "w")
  {
    save_state();
  }

  // save state and quit
  else if (keys.size() == 1 && keys.at(0) == "wq")
  {
    save_state();
    _ctx.is_running = false;
    return {};
  }

  else if (keys.at(0) == "prev" && (match_opt = OB::String::match(input,
    std::regex("^prev(?:\\s+([0-9]{1,2}))?$"))))
  {
    auto const match = match_opt.value().at(1);

    if (match.empty())
    {
      return std::make_pair(true, "prev " + std::to_string(_fltrdr.get_show_prev()));
    }
    else
    {
      auto const val = std::stoi(match);

      if (val < 0 || val > 60)
      {
        return std::make_pair(false, "error: value '" + std::to_string(val) + "' is out of range <0-60>");
      }

      _fltrdr.set_show_prev(val);
    }
  }

  else if (keys.at(0) == "next" && (match_opt = OB::String::match(input,
    std::regex("^next(?:\\s+([0-9]{1,2}))?$"))))
  {
    auto const match = match_opt.value().at(1);

    if (match.empty())
    {
      return std::make_pair(true, "next " + std::to_string(_fltrdr.get_show_next()));
    }
    else
    {
      auto const val = std::stoi(match);

      if (val < 0 || val > 60)
      {
        return std::make_pair(false, "error: value '" + std::to_string(val) + "' is out of range <0-60>");
      }

      _fltrdr.set_show_next(val);
    }
  }

  else if (keys.at(0) == "timer" && (match_opt = OB::String::match(input,
    std::regex("^timer(?:\\s+(clear|(?:(?:\\d+Y:)?(?:\\d+M:)?(?:\\d+W:)?(?:\\d+D:)?(?:\\d+h:)?(?:\\d+m:)?(?:\\d+s))))?$"))))
  {
    auto const match = OB::String::trim(match_opt.value().at(1));

    if (match.empty())
    {
      return std::make_pair(true, "timer " + _fltrdr.timer.str());
    }

    if (match == "clear")
    {
      _fltrdr.reset_timer();
    }
    else
    {
      _fltrdr.timer.str(match);
    }
  }

  // open
  else if (keys.at(0) == "open" && (match_opt = OB::String::match(input,
    std::regex("^open(?:\\s+([^\\r]+))?$"))))
  {
    fs::path const path = std::move(match_opt.value().at(1));

    if (path.empty())
    {
      return std::make_pair(true, "open " + _ctx.file.name);
    }

    if (! fs::exists(path))
    {
      return std::make_pair(false, "error: could not open file '" + path.string() + "'");
    }

    std::ifstream file {path};
    if (! file.is_open())
    {
      return std::make_pair(false, "error: could not open file '" + path.string() + "'");
    }

    if (_fltrdr.parse(file))
    {
      _ctx.file.path = path;
      _ctx.file.name = path.lexically_normal().string();
    }

    load_state();
  }

  // set wpm
  else if (keys.at(0) == "wpm" && (match_opt = OB::String::match(input,
    std::regex("^wpm(?:\\s+([0-9]+))?$"))))
  {
    auto const match = std::move(match_opt.value().at(1));

    if (match.empty())
    {
      return std::make_pair(true, "wpm " + std::to_string(_fltrdr.get_wpm()));
    }

    _fltrdr.set_wpm(std::stoi(match));
  }

  // set wpm-avg
  else if (keys.at(0) == "wpm-avg" && (match_opt = OB::String::match(input,
    std::regex("^wpm-avg(?:\\s+([0-9]+|clear))?$"))))
  {
    auto const match = std::move(match_opt.value().at(1));

    if (match.empty())
    {
      return std::make_pair(true, "wpm-avg " + std::to_string(_fltrdr.get_wpm_avg()));
    }

    if (match == "clear")
    {
      _fltrdr.reset_wpm_avg();
    }
    else
    {
      _fltrdr.set_wpm_avg(std::stoi(match));
    }
  }

  // goto word
  else if (keys.at(0) == "goto" && (match_opt = OB::String::match(input,
    std::regex("^goto(?:\\s+([0-9]+))?$"))))
  {
    auto const match = std::move(match_opt.value().at(1));

    if (match.empty())
    {
      return std::make_pair(true, "goto " + std::to_string(_fltrdr.get_index()));
    }

    _fltrdr.set_index(std::stoul(match));
  }

  // set offset
  else if (keys.at(0) == "offset" && (match_opt = OB::String::match(input,
    std::regex("^offset(?:\\s+([0-6]{1}))?$"))))
  {
    auto const match = std::move(match_opt.value().at(1));

    if (match.empty())
    {
      return std::make_pair(true, "offset " + std::to_string(_ctx.offset_value));
    }

    _ctx.offset_value = std::stoi(match);
  }

  else if (keys.size() >= 2 && keys.at(0) == "style")
  {
    // two-tone primary color
    if (keys.at(1) == "primary")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style primary " + _ctx.style.secondary.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.secondary = color;
      _ctx.style.border = color;
      _ctx.style.progress_fill = color;
      _ctx.style.word_primary = color;
      _ctx.style.prompt = color;

      color.bg();

      _ctx.style.background = color;
    }

    // two-tone secondary color
    else if (keys.at(1) == "secondary")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style secondary " + _ctx.style.primary.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.primary = color;
      _ctx.style.progress_bar = color;
      _ctx.style.word_secondary = color;
      _ctx.style.word_highlight = color;
      _ctx.style.word_punct = color;
      _ctx.style.word_quote = color;
    }

    // text color
    else if (keys.at(1) == "text")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style text " + _ctx.style.word_primary.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.word_primary = color;
      _ctx.style.word_secondary = color;
      _ctx.style.word_highlight = color;
      _ctx.style.word_punct = color;
      _ctx.style.word_quote = color;
    }

    else if (keys.at(1) == "status-background")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style status-background " + _ctx.style.background.key());
      }

      OB::Color color {keys.at(2), OB::Color::Type::bg};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.background = color;
    }

    else if (keys.at(1) == "background")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style background " + _ctx.style.bg.key());
      }

      OB::Color color {keys.at(2), OB::Color::Type::bg};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.bg = color;
    }

    else if (keys.at(1) == "countdown")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style countdown " + _ctx.style.countdown.key());
      }

      OB::Color color {keys.at(2), OB::Color::Type::bg};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.countdown = color;
    }

    else if (keys.at(1) == "status-primary")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style status-primary " + _ctx.style.primary.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.primary = color;
    }

    else if (keys.at(1) == "status-secondary")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style status-secondary " + _ctx.style.secondary.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.secondary = color;
    }

    else if (keys.at(1) == "border")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style border " + _ctx.style.border.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.border = color;
    }

    else if (keys.at(1) == "progress-bar")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style progress-bar " + _ctx.style.progress_bar.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.progress_bar = color;
    }

    else if (keys.at(1) == "progress-fill")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style progress-fill " + _ctx.style.progress_fill.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.progress_fill = color;
    }

    else if (keys.at(1) == "prompt")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style prompt " + _ctx.style.prompt.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.prompt = color;
    }

    else if (keys.at(1) == "success")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style success " + _ctx.style.success.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.success = color;
    }

    else if (keys.at(1) == "error")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style error " + _ctx.style.error.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.error = color;
    }

    else if (keys.at(1) == "text-primary")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style text-primary " + _ctx.style.word_primary.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.word_primary = color;
    }

    else if (keys.at(1) == "text-secondary")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style text-secondary " + _ctx.style.word_secondary.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.word_secondary = color;
    }

    else if (keys.at(1) == "text-highlight")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style text-highlight " + _ctx.style.word_highlight.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.word_highlight = color;
    }

    else if (keys.at(1) == "text-punct")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style text-punct " + _ctx.style.word_punct.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.word_punct = color;
    }

    else if (keys.at(1) == "text-quote")
    {
      if (keys.size() < 3)
      {
        return std::make_pair(true, "style text-quote " + _ctx.style.word_quote.key());
      }

      OB::Color color {keys.at(2)};

      if (! color)
      {
        return std::make_pair(false, "warning: unknown command '" + input + "'");
      }

      _ctx.style.word_quote = color;
    }

    else
    {
      return std::make_pair(false, "warning: unknown command '" + input + "'");
    }
  }

  else if (keys.size() >= 2 && keys.at(0) == "set")
  {
    if (match_opt = OB::String::match(input,
      std::regex("^set\\s+view(?:\\s+(true|false|t|f|1|0|on|off))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "set view " + std::to_string(static_cast<int>(_fltrdr.get_show_line())));
      }
      else if ("true" == match || "t" == match || "1" == match || "on" == match)
      {
        _fltrdr.set_show_line(true);
      }
      else
      {
        _fltrdr.set_show_line(false);
      }
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^set\\s+progress(?:\\s+(true|false|t|f|1|0|on|off))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "set progress " + std::to_string(static_cast<int>(_ctx.show.progress)));
      }
      else if ("true" == match || "t" == match || "1" == match || "on" == match)
      {
        _ctx.show.progress = true;
      }
      else
      {
        _ctx.show.progress = false;
      }
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^set\\s+status(?:\\s+(true|false|t|f|1|0|on|off))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "set status " + std::to_string(static_cast<int>(_ctx.show.status)));
      }
      else if ("true" == match || "t" == match || "1" == match || "on" == match)
      {
        _ctx.show.status = true;
      }
      else
      {
        _ctx.show.status = false;
      }
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^set\\s+border(?:\\s+(true|false|t|f|1|0|on|off))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "set border " + std::to_string(static_cast<int>(_ctx.show.border_top && _ctx.show.border_bottom)));
      }
      else if ("true" == match || "t" == match || "1" == match || "on" == match)
      {
        _ctx.show.border_top = true;
        _ctx.show.border_bottom = true;
      }
      else
      {
        _ctx.show.border_top = false;
        _ctx.show.border_bottom = false;
      }
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^set\\s+border\\-top(?:\\s+(true|false|t|f|1|0|on|off))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "set border-top " + std::to_string(static_cast<int>(_ctx.show.border_top)));
      }
      else if ("true" == match || "t" == match || "1" == match || "on" == match)
      {
        _ctx.show.border_top = true;
      }
      else
      {
        _ctx.show.border_top = false;
      }
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^set\\s+border\\-bottom(?:\\s+(true|false|t|f|1|0|on|off))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "set border-bottom " + std::to_string(static_cast<int>(_ctx.show.border_bottom)));
      }
      else if ("true" == match || "t" == match || "1" == match || "on" == match)
      {
        _ctx.show.border_bottom = true;
      }
      else
      {
        _ctx.show.border_bottom = false;
      }
    }

    else
    {
      return std::make_pair(false, "warning: unknown command '" + input + "'");
    }
  }

  else if (keys.size() >= 2 && keys.at(0) == "sym")
  {
    if (match_opt = OB::String::match(input,
      std::regex("^sym\\s+progress(?:\\s+(.{0,4}))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "sym progress " +
          (_ctx.sym.progress_bar == " " ? "clear" : _ctx.sym.progress_bar));
      }

      if (match == "clear")
      {
        _ctx.sym.progress_bar = " ";
        _ctx.sym.progress_fill = " ";

        return {};
      }

      OB::Text::View view {match};

      if (view.size() != 1 || view.cols() > 1 ||
        ! OB::Text::is_graph(OB::Text::to_int32(view.front())))
      {
        return std::make_pair(false, "error: invalid symbol '" + match + "'");
      }

      _ctx.sym.progress_bar = match;
      _ctx.sym.progress_fill = match;
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^sym\\s+progress\\-bar(?:\\s+(.{0,4}))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "sym progress-bar " +
          (_ctx.sym.progress_bar == " " ? "clear" : _ctx.sym.progress_bar));
      }

      if (match == "clear")
      {
        _ctx.sym.progress_bar = " ";

        return {};
      }

      OB::Text::View view {match};

      if (view.size() != 1 || view.cols() > 1 ||
        ! OB::Text::is_graph(OB::Text::to_int32(view.front())))
      {
        return std::make_pair(false, "error: invalid symbol '" + match + "'");
      }

      _ctx.sym.progress_bar = match;
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^sym\\s+progress\\-fill(?:\\s+(.{0,4}))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "sym progress-fill " +
          (_ctx.sym.progress_fill == " " ? "clear" : _ctx.sym.progress_fill));
      }

      if (match == "clear")
      {
        _ctx.sym.progress_fill = " ";

        return {};
      }

      OB::Text::View view {match};

      if (view.size() != 1 || view.cols() > 1 ||
        ! OB::Text::is_graph(OB::Text::to_int32(view.front())))
      {
        return std::make_pair(false, "error: invalid symbol '" + match + "'");
      }

      _ctx.sym.progress_fill = match;
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^sym\\s+border(?:\\s+(.{0,4}))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "sym border " +
          (_ctx.sym.border_top == " " ? "clear" : _ctx.sym.border_top));
      }

      if (match == "clear")
      {
        _ctx.sym.border_top = " ";
        _ctx.sym.border_top_mark = " ";
        _ctx.sym.border_bottom = " ";
        _ctx.sym.border_bottom_mark = " ";

        return {};
      }

      OB::Text::View view {match};

      if (view.size() != 1 || view.cols() > 1 ||
        ! OB::Text::is_graph(OB::Text::to_int32(view.front())))
      {
        return std::make_pair(false, "error: invalid symbol '" + match + "'");
      }

      _ctx.sym.border_top = match;
      _ctx.sym.border_top_mark = match;
      _ctx.sym.border_bottom = match;
      _ctx.sym.border_bottom_mark = match;
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^sym\\s+border\\-top(?:\\s+(.{0,4}))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "sym border-top " +
          (_ctx.sym.border_top == " " ? "clear" : _ctx.sym.border_top));
      }

      if (match == "clear")
      {
        _ctx.sym.border_top = " ";
        _ctx.sym.border_top_mark = " ";

        return {};
      }

      OB::Text::View view {match};

      if (view.size() != 1 || view.cols() > 1 ||
        ! OB::Text::is_graph(OB::Text::to_int32(view.front())))
      {
        return std::make_pair(false, "error: invalid symbol '" + match + "'");
      }

      _ctx.sym.border_top = match;
      _ctx.sym.border_top_mark = match;
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^sym\\s+border\\-top\\-line(?:\\s+(.{0,4}))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "sym border-top " +
          (_ctx.sym.border_top == " " ? "clear" : _ctx.sym.border_top));
      }

      if (match == "clear")
      {
        _ctx.sym.border_top = " ";

        return {};
      }

      OB::Text::View view {match};

      if (view.size() != 1 || view.cols() > 1 ||
        ! OB::Text::is_graph(OB::Text::to_int32(view.front())))
      {
        return std::make_pair(false, "error: invalid symbol '" + match + "'");
      }

      _ctx.sym.border_top = match;
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^sym\\s+border\\-top\\-mark(?:\\s+(.{0,4}))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "sym border-top-mark " +
          (_ctx.sym.border_top_mark == " " ? "clear" : _ctx.sym.border_top_mark));
      }

      if (match == "clear")
      {
        _ctx.sym.border_top_mark = " ";

        return {};
      }

      OB::Text::View view {match};

      if (view.size() != 1 || view.cols() > 1 ||
        ! OB::Text::is_graph(OB::Text::to_int32(view.front())))
      {
        return std::make_pair(false, "error: invalid symbol '" + match + "'");
      }

      _ctx.sym.border_top_mark = match;
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^sym\\s+border\\-bottom(?:\\s+(.{0,4}))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "sym border-bottom " +
          (_ctx.sym.border_bottom == " " ? "clear" : _ctx.sym.border_bottom));
      }

      if (match == "clear")
      {
        _ctx.sym.border_bottom = " ";
        _ctx.sym.border_bottom_mark = " ";

        return {};
      }

      OB::Text::View view {match};

      if (view.size() != 1 || view.cols() > 1 ||
        ! OB::Text::is_graph(OB::Text::to_int32(view.front())))
      {
        return std::make_pair(false, "error: invalid symbol '" + match + "'");
      }

      _ctx.sym.border_bottom = match;
      _ctx.sym.border_bottom_mark = match;
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^sym\\s+border\\-bottom\\-line(?:\\s+(.{0,4}))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "sym border-bottom-line " +
          (_ctx.sym.border_bottom == " " ? "clear" : _ctx.sym.border_bottom));
      }

      if (match == "clear")
      {
        _ctx.sym.border_bottom = " ";

        return {};
      }

      OB::Text::View view {match};

      if (view.size() != 1 || view.cols() > 1 ||
        ! OB::Text::is_graph(OB::Text::to_int32(view.front())))
      {
        return std::make_pair(false, "error: invalid symbol '" + match + "'");
      }

      _ctx.sym.border_bottom = match;
    }

    else if (match_opt = OB::String::match(input,
      std::regex("^sym\\s+border\\-bottom\\-mark(?:\\s+(.{0,4}))?$")))
    {
      auto const match = match_opt.value().at(1);

      if (match.empty())
      {
        return std::make_pair(true, "sym border-bottom-mark " +
          (_ctx.sym.border_bottom_mark == " " ? "clear" : _ctx.sym.border_bottom_mark));
      }

      if (match == "clear")
      {
        _ctx.sym.border_bottom_mark = " ";

        return {};
      }

      OB::Text::View view {match};

      if (view.size() != 1 || view.cols() > 1 ||
        ! OB::Text::is_graph(OB::Text::to_int32(view.front())))
      {
        return std::make_pair(false, "error: invalid symbol '" + match + "'");
      }

      _ctx.sym.border_bottom_mark = match;
    }

    else
    {
      return std::make_pair(false, "warning: unknown command '" + input + "'");
    }
  }

  // unknown
  else
  {
    return std::make_pair(false, "warning: unknown command '" + input + "'");
  }

  return {};
}

void Tui::command_prompt()
{
  // reset prompt message count
  _ctx.prompt.count = 0;

  // set prompt style
  _readline.style(_ctx.style.secondary.value() + _ctx.style.bg.value());
  _readline.prompt(":", _ctx.style.prompt.value() + _ctx.style.bg.value());

  std::cout
  << aec::cursor_save
  << aec::cursor_set(0, _ctx.height)
  << aec::erase_line
  << aec::cursor_show
  << std::flush;

  // read user input
  auto input = _readline(_ctx.is_running);

  std::cout
  << aec::cursor_hide
  << aec::cursor_load
  << std::flush;

  if (auto const res = command(input))
  {
    set_status(res.value().first, res.value().second);
  }
}

void Tui::search_forward()
{
  // reset prompt message count
  _ctx.prompt.count = 0;

  // set prompt style
  _readline_search.style(_ctx.style.secondary.value() + _ctx.style.bg.value());
  _readline_search.prompt("/", _ctx.style.prompt.value() + _ctx.style.bg.value());

  std::cout
  << aec::cursor_save
  << aec::cursor_set(0, _ctx.height)
  << aec::erase_line
  << aec::cursor_show
  << std::flush;

  // read user input
  auto input {_readline_search(_ctx.is_running)};

  std::cout
  << aec::cursor_hide
  << aec::cursor_load
  << std::flush;

  if (! _ctx.is_running)
  {
    _ctx.is_running = false;

    return;
  }
  else if (! input.empty() && ! _fltrdr.search(input, true))
  {
    set_status(false, input);
  }
}

void Tui::search_backward()
{
  // reset prompt message count
  _ctx.prompt.count = 0;

  // set prompt style
  _readline_search.style(_ctx.style.secondary.value() + _ctx.style.bg.value());
  _readline_search.prompt("?", _ctx.style.prompt.value() + _ctx.style.bg.value());

  std::cout
  << aec::cursor_save
  << aec::cursor_set(0, _ctx.height)
  << aec::erase_line
  << aec::cursor_show
  << std::flush;

  // read user input
  auto input {_readline_search(_ctx.is_running)};

  std::cout
  << aec::cursor_hide
  << aec::cursor_load
  << std::flush;

  if (! _ctx.is_running)
  {
    _ctx.is_running = false;

    return;
  }
  else if (! input.empty() && ! _fltrdr.search(input, false))
  {
    set_status(false, input);
  }
}

int Tui::screen_size()
{
  bool width_invalid {_ctx.width < _ctx.width_min};
  bool height_invalid {_ctx.height < _ctx.height_min};

  if (width_invalid || height_invalid)
  {
    clear();

    _ctx.buf
    << _ctx.style.bg
    << _ctx.style.error;

    if (width_invalid && height_invalid)
    {
      _ctx.buf
      << "Error: width "
      << _ctx.width
      << " (min "
      << _ctx.width_min
      << ") height "
      << _ctx.height
      << " (min "
      << _ctx.height_min
      << ")";
    }
    else if (width_invalid)
    {
      _ctx.buf
      << "Error: width "
      << _ctx.width
      << " (min "
      << _ctx.width_min
      << ")";
    }
    else
    {
      _ctx.buf
      << "Error: height "
      << _ctx.height
      << " (min "
      << _ctx.height_min
      << ")";
    }

    _ctx.buf
    << aec::clear;

    refresh();

    return 1;
  }

  return 0;
}
