#ifndef OB_TEXT_HH
#define OB_TEXT_HH

#define U_CHARSET_IS_UTF8 1

#include <unicode/regex.h>
#include <unicode/utext.h>
#include <unicode/brkiter.h>

#include <cstddef>
#include <cstdint>

#include <string>
#include <string_view>
#include <vector>
#include <limits>
#include <memory>
#include <utility>
#include <iterator>
#include <algorithm>

namespace OB
{

template<typename T>
class Basic_Text
{
public:

  using size_type = std::size_t;
  using char_type = T;
  using string = std::basic_string<char_type>;
  using string_view = std::basic_string_view<char_type>;
  using brk_iter = icu::BreakIterator;
  using locale = icu::Locale;

  struct Ctx
  {
    Ctx(size_type bytes_, size_type cols_, string_view str_) :
      bytes {bytes_},
      cols {cols_},
      str {str_}
    {
    }

    friend std::ostream& operator<<(std::ostream& os, Ctx const& obj)
    {
      os << obj.str;

      return os;
    }

    operator string()
    {
      return string(str);
    }

    operator string_view()
    {
      return str;
    }

    size_type bytes {0};
    size_type cols {0};
    string_view str {};
  }; // struct Ctx

  using value_type = std::vector<Ctx>;
  using iterator = typename value_type::iterator;
  using const_iterator = typename value_type::const_iterator;
  using reverse_iterator = typename value_type::reverse_iterator;
  using const_reverse_iterator = typename value_type::const_reverse_iterator;

  static auto constexpr iter_end {icu::BreakIterator::DONE};
  static size_type constexpr npos {std::numeric_limits<size_type>::max()};

  Basic_Text() = default;
  Basic_Text(Basic_Text<T>&&) = default;
  Basic_Text(Basic_Text<T> const&) = default;

  Basic_Text(string_view str)
  {
    this->str(str);
  }

  ~Basic_Text() = default;

  Basic_Text& operator=(Basic_Text<T>&&) = default;
  Basic_Text& operator=(Basic_Text<T> const& obj) = default;

  Basic_Text& operator=(string_view str)
  {
    this->str(str);

    return *this;
  }

  friend std::ostream& operator<<(std::ostream& os, Basic_Text<T> const& obj)
  {
    os << obj.str();

    return os;
  }

  operator string()
  {
    return string(str());
  }

  bool empty() const
  {
    return _str.empty();
  }

  Basic_Text<T>& str(string_view str)
  {
    _cols = 0;
    _size = 0;
    _bytes = 0;

    _str.clear();
    _str.shrink_to_fit();

    if (str.empty())
    {
      return *this;
    }

    UErrorCode ec = U_ZERO_ERROR;

    std::unique_ptr<UText, decltype(&utext_close)> text (
      utext_openUTF8(nullptr, str.data(), static_cast<std::int64_t>(str.size()), &ec),
      utext_close);

    if (U_FAILURE(ec))
    {
      throw std::runtime_error("failed to create utext");
    }

    std::unique_ptr<brk_iter> iter {brk_iter::createCharacterInstance(
      locale::getDefault(), ec)};

    if (U_FAILURE(ec))
    {
      throw std::runtime_error("failed to create break iterator");
    }

    iter->setText(text.get(), ec);

    if (U_FAILURE(ec))
    {
      throw std::runtime_error("failed to create break iterator");
    }

    // get size of iterator
    size_type size {0};
    while (iter->next() != iter_end)
    {
      ++size;
    }

    // reserve array size
    _str.reserve(size);

    size = 0;
    UChar32 uch;
    int width {0};
    size_type cols {0};
    auto begin = iter->first();
    auto end = iter->next();

    while (end != iter_end)
    {
      // increase total size count
      ++_size;

      // get column width
      uch = utext_char32At(text.get(), begin);
      width = u_getIntPropertyValue(uch, UCHAR_EAST_ASIAN_WIDTH);
      if (width == U_EA_FULLWIDTH || width == U_EA_WIDE)
      {
        // full width
        cols = 2;
      }
      else
      {
        // half width
        cols = 1;
      }

      // increase total column count
      _cols += cols;

      // get string size
      size = static_cast<size_type>(end - begin);

      // add character context to array
      _str.emplace_back(_bytes, cols, string_view(str.data() + (_bytes * sizeof(char_type)), size));

      // increase total byte count
      _bytes += size;

      // increase iterators
      begin = end;
      end = iter->next();
    }

    return *this;
  }

  string_view str() const
  {
    if (_str.empty())
    {
      return {};
    }

    return string_view(_str.at(0).str.data(), _bytes);
  }

  value_type const& get() const
  {
    return _str;
  }

  size_type byte_to_char(size_type pos) const
  {
    if (pos >= _bytes)
    {
      return npos;
    }

    auto const it = std::lower_bound(_str.crbegin(), _str.crend(), pos,
      [](auto const& lhs, auto const& rhs) {
        return lhs.bytes > rhs;
      });

    if (it != _str.crend())
    {
      return static_cast<size_type>(std::distance(_str.cbegin(), it.base()) - 1);
    }

    return npos;
  }

  Ctx& at(size_type pos)
  {
    return _str.at(pos);
  }

  Ctx const& at(size_type pos) const
  {
    return _str.at(pos);
  }

  string_view substr(size_type pos, size_type size = npos) const
  {
    if (pos >= _size)
    {
      return {};
    }

    if (size == npos)
    {
      size = _size;
    }
    else
    {
      size += pos;
    }

    size_type count {0};

    for (size_type i = pos; i < size && i < _size; ++i)
    {
      count += _str.at(i).str.size();
    }

    return string_view(_str.at(pos).str.data(), count);
  }

  size_type find(string const& str, size_type pos = npos)
  {
    if (pos == npos)
    {
      pos = 0;
    }

    if (pos >= _size)
    {
      return npos;
    }


    for (size_type i = pos; i < _size; ++i)
    {
      if (str == _str.at(i).str)
      {
        return i;
      }
    }

    return npos;
  }

  size_type rfind(string const& str, size_type pos = npos)
  {
    if (pos == npos)
    {
      pos = _size - 1;
    }

    if (pos == 0 || pos >= _size)
    {
      return npos;
    }

    for (size_type i = pos; i != npos; --i)
    {
      if (str == _str.at(i).str)
      {
        return i;
      }
    }

    return npos;
  }

  size_type find_first_of(Basic_Text<T> const& str, size_type pos = npos)
  {
    if (pos == npos)
    {
      pos = 0;
    }

    if (pos >= _size)
    {
      return npos;
    }

    for (size_type i = pos; i < _size; ++i)
    {
      auto const& lhs = _str.at(i).str;

      for (size_type j = 0; j < str.size(); ++j)
      {
        if (lhs == str.at(j).str)
        {
          return i;
        }
      }
    }

    return npos;
  }

  size_type rfind_first_of(Basic_Text<T> const& str, size_type pos = npos)
  {
    if (pos == npos)
    {
      pos = _size - 1;
    }

    if (pos == 0 || pos >= _size)
    {
      return npos;
    }

    for (size_type i = pos; i != npos; --i)
    {
      auto const& lhs = _str.at(i).str;

      for (size_type j = 0; j < str.size(); ++j)
      {
        if (lhs == str.at(j).str)
        {
          return i;
        }
      }
    }

    return npos;
  }

  Basic_Text<T>& clear()
  {
    _str.clear();
    _bytes = 0;
    _cols = 0;
    _size = 0;

    return *this;
  }

  Basic_Text<T>& shrink_to_fit()
  {
    _str.shrink_to_fit();

    return *this;
  }

  size_type cols() const
  {
    return _cols;
  }

  size_type size() const
  {
    return _size;
  }

  size_type length() const
  {
    return _size;
  }

  size_type bytes() const
  {
    return _bytes;
  }

  iterator begin()
  {
    return _str.begin();
  }

  const_iterator cbegin() const
  {
    return _str.cbegin();
  }

  reverse_iterator rbegin()
  {
    return _str.rbegin();
  }

  const_reverse_iterator crbegin() const
  {
    return _str.crbegin();
  }

  iterator end()
  {
    return _str.end();
  }

  const_iterator cend() const
  {
    return _str.cend();
  }

  reverse_iterator rend()
  {
    return _str.rend();
  }

  const_reverse_iterator crend() const
  {
    return _str.crend();
  }

  static std::int32_t to_int32(string_view str)
  {
    UErrorCode ec = U_ZERO_ERROR;

    std::unique_ptr<UText, decltype(&utext_close)> text (
      utext_openUTF8(nullptr, str.data(), static_cast<std::int64_t>(str.size()), &ec),
      utext_close);

    if (U_FAILURE(ec))
    {
      throw std::runtime_error("failed to create utext");
    }

    return utext_char32At(text.get(), 0);
  }

  static bool is_quote(std::int32_t ch)
  {
    switch(ch)
    {
      case U'"': case U'\'': case U'«': case U'»': case U'‘': case U'’':
      case U'‚': case U'‛': case U'“': case U'”': case U'„': case U'‟':
      case U'‹': case U'›': case U'❛': case U'❜': case U'❝': case U'❞':
      case U'❟': case U'❮': case U'❯': case U'⹂': case U'「': case U'」':
      case U'『': case U'』': case U'〝': case U'〞': case U'〟': case U'＂':
        return true;

      default:
        return false;
    }
  }

  static bool is_upper(std::int32_t ch)
  {
    return u_isupper(ch);
  }

  static bool is_lower(std::int32_t ch)
  {
    return u_islower(ch);
  }

  static bool is_punct(std::int32_t ch)
  {
    return u_ispunct(ch);
  }

  static bool is_digit(std::int32_t ch)
  {
    return u_isdigit(ch);
  }

  static bool is_alpha(std::int32_t ch)
  {
    return u_isalpha(ch);
  }

  static bool is_alnum(std::int32_t ch)
  {
    return u_isalnum(ch);
  }

  static bool is_xdigit(std::int32_t ch)
  {
    return u_isxdigit(ch);
  }

  static bool is_blank(std::int32_t ch)
  {
    return u_isblank(ch);
  }

  static bool is_space(std::int32_t ch)
  {
    return u_isspace(ch);
  }

  static bool is_Whitespace(std::int32_t ch)
  {
    return u_isWhitespace(ch);
  }

  static bool is_cntrl(std::int32_t ch)
  {
    return u_iscntrl(ch);
  }

  static bool to_upper(std::int32_t ch)
  {
    return u_toupper(ch);
  }

  static bool to_lower(std::int32_t ch)
  {
    return u_tolower(ch);
  }

private:

  // array of contexts mapping the string
  value_type _str;

  // number of columns needed to display the string
  size_type _cols {0};

  // number of grapheme clusters in the string
  size_type _size {0};

  // number of bytes in the string
  size_type _bytes {0};
}; // class Text

using Text = Basic_Text<char>;

class Regex
{
public:

  using size_type = std::size_t;
  using char_type = char;
  using string = std::basic_string<char_type>;
  using string_view = std::basic_string_view<char_type>;
  using regex = icu::RegexMatcher;

  struct Match
  {
    friend std::ostream& operator<<(std::ostream& os, Match const& obj)
    {
      os << obj.str;

      return os;
    }

    size_type pos {0};
    size_type size {0};
    string_view str;
    std::vector<string_view> group;
  }; // struct Match

  using value_type = std::vector<Match>;
  using iterator = typename value_type::iterator;
  using const_iterator = typename value_type::const_iterator;
  using reverse_iterator = typename value_type::reverse_iterator;
  using const_reverse_iterator = typename value_type::const_reverse_iterator;

  Regex() = default;
  Regex(Regex&&) = default;
  Regex(Regex const&) = default;

  Regex(string_view rx, string_view str)
  {
    match(rx, str);
  }

  ~Regex() = default;

  Regex& operator=(Regex&&) = default;
  Regex& operator=(Regex const& obj) = default;

  Regex& match(string_view rx, string_view str)
  {
    _size = 0;

    _str.clear();
    _str.shrink_to_fit();

    if (str.empty())
    {
      return *this;
    }

    UErrorCode ec = U_ZERO_ERROR;

    std::unique_ptr<UText, decltype(&utext_close)> urx (
      utext_openUTF8(nullptr, rx.data(), static_cast<std::int64_t>(rx.size()), &ec),
      utext_close);

    if (U_FAILURE(ec))
    {
      throw std::runtime_error("failed to create utext");
    }

    std::unique_ptr<UText, decltype(&utext_close)> ustr (
      utext_openUTF8(nullptr, str.data(), static_cast<std::int64_t>(str.size()), &ec),
      utext_close);

    if (U_FAILURE(ec))
    {
      throw std::runtime_error("failed to create utext");
    }

    std::unique_ptr<regex> iter {new regex(urx.get(), UREGEX_CASE_INSENSITIVE, ec)};

    if (! U_SUCCESS(ec))
    {
      throw std::runtime_error("failed to create regex matcher");
    }

    iter->reset(ustr.get());

    size_type size {0};
    std::int32_t count {0};
    std::int32_t begin {0};
    std::int32_t end {0};

    while (iter->find())
    {
      count = static_cast<size_type>(iter->groupCount());

      begin = iter->start(ec);

      if (U_FAILURE(ec))
      {
        throw std::runtime_error("failed to get regex matcher start");
      }

      end = iter->end(ec);

      if (U_FAILURE(ec))
      {
        throw std::runtime_error("failed to get regex matcher end");
      }

      size = static_cast<size_type>(end - begin);

      Match match;
      match.pos = static_cast<size_type>(begin);
      match.size = static_cast<size_type>(count);
      match.str = string_view(str.data() + begin, size);

      for (std::int32_t i = 1; i <= count; ++i)
      {
        begin = iter->start(i, ec);

        if (U_FAILURE(ec))
        {
          throw std::runtime_error("failed to get regex matcher group start");
        }

        end = iter->end(i, ec);

        if (U_FAILURE(ec))
        {
          throw std::runtime_error("failed to get regex matcher group end");
        }

        size = static_cast<size_type>(end - begin);

        match.group.emplace_back(string_view(str.data() + begin, size));
      }

      _str.emplace_back(match);
    }

    _size = _str.size();

    return *this;
  }

  value_type const& get() const
  {
    return _str;
  }

  Match& at(size_type pos)
  {
    return _str.at(pos);
  }

  Match const& at(size_type pos) const
  {
    return _str.at(pos);
  }

  bool empty() const
  {
    return _str.empty();
  }

  Regex& clear()
  {
    _str.clear();
    _size = 0;

    return *this;
  }

  Regex& shrink_to_fit()
  {
    _str.shrink_to_fit();

    return *this;
  }

  size_type size() const
  {
    return _size;
  }

  size_type length() const
  {
    return _size;
  }

  iterator begin()
  {
    return _str.begin();
  }

  const_iterator cbegin() const
  {
    return _str.cbegin();
  }

  reverse_iterator rbegin()
  {
    return _str.rbegin();
  }

  const_reverse_iterator crbegin() const
  {
    return _str.crbegin();
  }

  iterator end()
  {
    return _str.end();
  }

  const_iterator cend() const
  {
    return _str.cend();
  }

  reverse_iterator rend()
  {
    return _str.rend();
  }

  const_reverse_iterator crend() const
  {
    return _str.crend();
  }

private:

  value_type _str;
  size_type _size {0};
}; // class Regex

class UString
{
public:

  UString(std::string const& val = {}):
    str {val},
    txt {str}
  {
  }

  UString& sync()
  {
    txt.str(str);

    return *this;
  }

  std::string str;
  Text txt;
}; // class UString

} // namespace OB

#endif // OB_TEXT_HH
