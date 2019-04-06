#ifndef OB_TIMER_HH
#define OB_TIMER_HH

#include "ob/string.hh"

#include <ctime>

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

namespace OB
{

class Timer
{
public:

  Timer() = default;

  operator bool()
  {
    return _is_running;
  }

  Timer& start()
  {
    _is_running = true;
    _start = std::chrono::high_resolution_clock::now();

    return *this;
  }

  Timer& stop()
  {
    update();
    _is_running = false;

    return *this;
  }

  Timer& reset()
  {
    _is_running = false;
    _total = {};

    return *this;
  }

  template<typename T>
  T time()
  {
    if (_is_running)
    {
      update();
    }

    return std::chrono::duration_cast<T>(_total);
  }

  std::string str()
  {
    if (_is_running)
    {
      update();
    }

    long int const diff {std::chrono::time_point_cast<std::chrono::seconds>(_total).time_since_epoch().count()};

    return seconds_to_string(diff);
  }

  void str(std::string const& str)
  {
    reset();
    _total = std::chrono::time_point<std::chrono::high_resolution_clock>(string_to_seconds(str));
  }

private:

  void update()
  {
    auto const stop = std::chrono::high_resolution_clock::now();

    if (stop > _start)
    {
      _total += (stop - _start);
    }

    _start = stop;
  }

  std::chrono::seconds string_to_seconds(std::string const& str)
  {
    long int constexpr t_second {1};
    long int constexpr t_minute {t_second * 60};
    long int constexpr t_hour   {t_minute * 60};
    long int constexpr t_day    {t_hour * 24};
    long int constexpr t_week   {t_day * 7};
    long int constexpr t_month  (t_day * 30.4);
    long int constexpr t_year   {t_month * 12};

    auto const pstr = OB::String::match(str,
      std::regex("^(?:(\\d+)Y:)?(?:(\\d+)M:)?(?:(\\d+)W:)?(?:(\\d+)D:)?(?:(\\d+)h:)?(?:(\\d+)m:)?(?:(\\d+)s)$"));

    if (! pstr)
    {
      return static_cast<std::chrono::seconds>(0);
    }

    auto const t = pstr.value();
    long int sec {0};

    if (! t.at(1).empty())
    {
      sec += std::stol(t.at(1)) * t_year;
    }

    if (! t.at(2).empty())
    {
      sec += std::stol(t.at(2)) * t_month;
    }

    if (! t.at(3).empty())
    {
      sec += std::stol(t.at(3)) * t_week;
    }

    if (! t.at(4).empty())
    {
      sec += std::stol(t.at(4)) * t_day;
    }

    if (! t.at(5).empty())
    {
      sec += std::stol(t.at(5)) * t_hour;
    }

    if (! t.at(6).empty())
    {
      sec += std::stol(t.at(6)) * t_minute;
    }

    if (! t.at(7).empty())
    {
      sec += std::stol(t.at(7)) * t_second;
    }

    return static_cast<std::chrono::seconds>(sec);
  }

  std::string seconds_to_string(long int sec)
  {
    std::string res;

    long int constexpr t_second {1};
    long int constexpr t_minute {t_second * 60};
    long int constexpr t_hour   {t_minute * 60};
    long int constexpr t_day    {t_hour * 24};
    long int constexpr t_week   {t_day * 7};
    long int constexpr t_month  (t_day * 30.4);
    long int constexpr t_year   {t_month * 12};

    auto const fuzzy_string = [&](long int const time_ref, std::string const time_str)
    {
      auto const t = (sec / time_ref);
      sec -= (t * time_ref);
      res += std::to_string(t) + time_str + ":";
    };

    if (sec >= t_year)
    {
      fuzzy_string(t_year, "Y");
    }
    if (sec >= t_month)
    {
      fuzzy_string(t_month, "M");
    }
    if (sec >= t_week)
    {
      fuzzy_string(t_week, "W");
    }
    if (sec >= t_day)
    {
      fuzzy_string(t_day, "D");
    }
    if (sec >= t_hour)
    {
      fuzzy_string(t_hour, "h");
    }
    if (sec >= t_minute)
    {
      fuzzy_string(t_minute, "m");
    }
    if (sec >= t_second)
    {
      fuzzy_string(t_second, "s");
    }
    else
    {
      res += "0s:";
    }

    res.pop_back();

    return res;
  }

  bool _is_running {false};
  std::chrono::time_point<std::chrono::high_resolution_clock> _start;
  std::chrono::time_point<std::chrono::high_resolution_clock> _total;
}; // class Timer

} // namespace OB

#endif // OB_TIMER_HH
