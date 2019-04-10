#ifndef OB_COLOR_HH
#define OB_COLOR_HH

#include "ob/string.hh"
#include "ob/term.hh"
namespace aec = OB::Term::ANSI_Escape_Codes;

#include <string>
#include <unordered_map>

namespace OB
{

class Color
{
  inline static const std::unordered_map<std::string, std::string> color_fg {
    {"black", "\x1b[30m"},
    {"black bright", "\x1b[90m"},
    {"red", "\x1b[31m"},
    {"red bright", "\x1b[91m"},
    {"green", "\x1b[32m"},
    {"green bright", "\x1b[92m"},
    {"yellow", "\x1b[33m"},
    {"yellow bright", "\x1b[93m"},
    {"blue", "\x1b[34m"},
    {"blue bright", "\x1b[94m"},
    {"magenta", "\x1b[35m"},
    {"magenta bright", "\x1b[95m"},
    {"cyan", "\x1b[36m"},
    {"cyan bright", "\x1b[96m"},
    {"white", "\x1b[37m"},
    {"white bright", "\x1b[97m"},
  };

  inline static const std::unordered_map<std::string, std::string> color_bg {
    {"black", "\x1b[40m"},
    {"black bright", "\x1b[100m"},
    {"red", "\x1b[41m"},
    {"red bright", "\x1b[101m"},
    {"green", "\x1b[42m"},
    {"green bright", "\x1b[102m"},
    {"yellow", "\x1b[43m"},
    {"yellow bright", "\x1b[103m"},
    {"blue", "\x1b[44m"},
    {"blue bright", "\x1b[104m"},
    {"magenta", "\x1b[45m"},
    {"magenta bright", "\x1b[105m"},
    {"cyan", "\x1b[46m"},
    {"cyan bright", "\x1b[106m"},
    {"white", "\x1b[47m"},
    {"white bright", "\x1b[107m"},
  };

public:

  struct Type
  {
    enum value
    {
      bg = 0,
      fg
    };
  };

  Color() = default;

  Color(Type::value const fg) noexcept :
    _fg {static_cast<bool>(fg)}
  {
  }

  Color(std::string const& k, Type::value const fg = Type::value::fg) :
    _fg {static_cast<bool>(fg)}
  {
    key(k);
  }

  Color(Color&&) = default;
  Color(Color const&) = default;
  ~Color() = default;

  Color& operator=(Color&& rhs) = default;
  Color& operator=(Color const&) = default;

  Color& operator=(std::string const& k)
  {
    clear();
    key(k);

    return *this;
  }

  operator bool() const
  {
    return _valid;
  }

  operator std::string() const
  {
    return _key;
  }

  friend std::ostream& operator<<(std::ostream& os, Color const& obj)
  {
    os << obj._value;

    return os;
  }

  friend std::string& operator+=(std::string& str, Color const& obj)
  {
    return str += obj._value;
  }

  bool is_valid() const
  {
    return _valid;
  }

  Color& clear()
  {
    _key = "clear";
    _value.clear();
    _valid = false;

    return *this;
  }

  Color& fg()
  {
    if (! _fg)
    {
      _fg = true;
      key(_key);
    }

    return *this;
  }

  Color& bg()
  {
    if (_fg)
    {
      _fg = false;
      key(_key);
    }

    return *this;
  }

  bool is_fg() const
  {
    return _fg;
  }

  bool is_bg() const
  {
    return ! _fg;
  }

  std::string key() const
  {
    return _key;
  }

  bool key(std::string const& k)
  {
    if (! k.empty())
    {
      if (k == "clear")
      {
        // clear
        _key = k;
        _value = "";
        _valid = true;
      }
      else if (k == "reverse")
      {
        // reverse
        _key = k;
        _value = "\x1b[7m";
        _valid = true;
      }
      else
      {
        // 21-bit color
        if (k.at(0) == '#' && OB::String::assert_rx(k,
          std::regex("^#[0-9a-fA-F]{3}(?:[0-9a-fA-F]{3})?$")))
        {
          _key = k;
          if (_fg)
          {
            _value = aec::fg_true(k);
          }
          else
          {
            _value = aec::bg_true(k);
          }
          _valid = true;
        }

        // 8-bit color
        else if (OB::String::assert_rx(k, std::regex("^[0-9]{1,3}$")) &&
          std::stoi(k) >= 0 && std::stoi(k) <= 255)
        {
          _key = k;
          if (_fg)
          {
            _value = aec::fg_256(k);
          }
          else
          {
            _value = aec::bg_256(k);
          }
          _valid = true;
        }

        // 4-bit color
        else if (color_fg.find(k) != color_fg.end())
        {
          _key = k;
          if (_fg)
          {
            _value = color_fg.at(k);
          }
          else
          {
            _value = color_bg.at(k);
          }
          _valid = true;
        }
      }
    }

    return _valid;
  }

  std::string value() const
  {
    return _value;
  }

private:

  bool _valid {false};
  bool _fg {true};
  std::string _key {"clear"};
  std::string _value;
}; // Color

} // namespace OB

#endif // OB_COLOR_HH
