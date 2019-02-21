#include "ob/parg.hh"
using Parg = OB::Parg;

#include "ob/term.hh"
namespace aec = OB::Term::ANSI_Escape_Codes;

#include "fltrdr/tui.hh"

#include <fcntl.h>
#include <unistd.h>

#include <cstddef>

#include <string>
#include <sstream>
#include <iostream>

// prototypes
int program_options(Parg& pg);

int program_options(Parg& pg)
{
  pg.name("fltrdr").version("0.1.1 (21.02.2019)");
  pg.description("A TUI text reader for the terminal.");

  pg.usage("[--config=<path>] [<file>]");
  pg.usage("[--help|-h]");
  pg.usage("[--version|-v]");
  pg.usage("[--license]");

  pg.info("Key Bindings", {
    "q|Q|<ctrl-c>\n    quit the program",
    ":\n    enter the command prompt",
    "/\n    search forwards prompt",
    "?\n    search backwards prompt",
    "<esc>\n    pause or discard prompt",
    "<space>\n    toggle play/pause",
    "gg\n    goto beginning",
    "G\n    goto end",
    "v\n    toggle view",
    "i\n    increase show prev word",
    "I\n    decrease show prev word",
    "o\n    increase show next word",
    "O\n    decrease show next word",
    "*\n    search next current word",
    "#\n    search prev current word",
    "n\n    goto next search result",
    "N\n    goto prev search result",
    "h|<left>\n    goto prev word",
    "l|<right>\n    goto next word",
    "H\n    goto prev sentence",
    "L\n    goto next sentence",
    "j|<down>\n    decrease wpm",
    "k|<up>\n    increase wpm",
    "J\n    goto prev chapter",
    "K\n    goto next chapter",
  });

  pg.info("Commands", {
    "quit\n    quit the program",
    "open <path>\n    open file for reading",
    "wpm <60-1200>\n    set wpm value",
    "goto <int>\n    goto specified word number",
    "prev <0-8>\n    set number of prev words to show",
    "next <0-8>\n    set number of next words to show",
    "offset <0-6>\n    set offset of focus point from center",

    R"RAW(
  reset <value>
    wpm
      reset wpm average
    timer
      reset timer
)RAW",

    R"RAW(set <value> <on|off>
    view
      toggle full text line visibility
    progress
      toggle progress bar visibility
    status
      toggle status bar visibility
    border
      toggle border top and bottom visibility
    border-top
      toggle border top visibility
    border-bottom
      toggle border bottom visibility
)RAW",

    R"RAW(sym <value> <char|unicode-char>
    progress
      progress bar symbol
    border-top
      border top symbol
    border-top-mark
      border top mark symbol
    border-top-line
      border top and border top mark symbol
    border-bottom
      border bottom symbol
    border-bottom-mark
      border bottom mark symbol
    border-bottom-line
      border bottom and border bottom mark symbol
)RAW",

    R"RAW(style <value> <#000000-#ffffff|0-255|Colour>
    primary
      meta style, sets the primary colour of the program
    secondary
      meta style, sets the secondary colour of the program
    text
      meta style, sets the colours of all input text shown in the reader
    countdown
      set the text background colour shown to countdown
      after play is pressed and before the reader begins
    border
      set the colour of the border surrounding the reader
    prompt
      set the colour of the command prompt symbol shown at the start of the line
    success
      set the success status message colour
    error
      set the error status message colour
    progress-primary
      set the progress bar primary colour
    progress-secondary
      set progress bar secondary colour
    status-primary
      set the colour of the text in the status bar
    status-secondary
      set the colour of the file path in the status bar
    status-background
      set the background colour of the status bar
    text-primary
      set the colour of the current word in the reader
    text-secondary
      set the colour of the words shown on either side of the current word
    text-highlight
      set the colour of the focus point character highlighted in the current word
    text-punct
      set the colour of the punctuation shown in the reader
    text-quote
      set the colour of the quotes shown in the reader)RAW",
  });

  pg.info("Colour", {
    "black [bright]",
    "red [bright]",
    "green [bright]",
    "yellow [bright]",
    "blue [bright]",
    "magenta [bright]",
    "cyan [bright]",
    "white [bright]",
  });

  pg.info("Configuration", {
    R"RAW(The default config locations are ${XDG_CONFIG_HOME}/fltrdr/config and
  ${HOME}/.fltrdr/config. A custom path can also be passed to override
  the default locations using the --config option. The config directory
  and file must be created by the user. If the file does not exist, the
  program continues as normal.

  The file, 'config', is a plain text file that can contain any of the
  commands listed in the Commands section of the --help output.
  Each command must be on its own line. Lines that begin with the '#'
  character are treated as comments.)RAW"
  });

  pg.info("Config Directory Locations", {
    "${XDG_CONFIG_HOME}/fltrdr",
    "${HOME}/.fltrdr",
  });

  pg.info("Config File Locations", {
    "${XDG_CONFIG_HOME}/fltrdr/config",
    "${HOME}/.fltrdr/config",
    "custom path with '--config=<path>'"
  });

  pg.info("Examples", {
    "fltrdr",
    "fltrdr <file>",
    "cat <file> | fltrdr",
    "fltrdr --config './path/to/config'",
    "fltrdr --help",
    "fltrdr --version",
    "fltrdr --license",
  });

  pg.info("Exit Codes", {"0 -> normal", "1 -> error"});

  pg.info("Repository", {
    "https://github.com/octobanana/fltrdr.git",
  });

  pg.info("Homepage", {
    "https://octobanana.com/software/fltrdr",
  });

  pg.author("Brett Robinson (octobanana) <octobanana.dev@gmail.com>");

  // general flags
  pg.set("help,h", "print the help output");
  pg.set("version,v", "print the program version");
  pg.set("license", "print the program license");

  // options
  pg.set("config", "", "path", "custom path to config file");

  pg.set_pos();

  int status {pg.parse()};

  if (status < 0)
  {
    std::cerr << "Usage:\n" << pg.usage() << "\n";
    std::cerr << "Error: " << pg.error() << "\n";

    auto const similar_names = pg.similar();
    if (similar_names.size() > 0)
    {
      std::cerr
      << "did you mean:\n";
      for (auto const& e : similar_names)
      {
        std::cerr
        << "  --" << e << "\n";
      }
    }

    return -1;
  }

  if (pg.get<bool>("help"))
  {
    std::cout << pg.help();

    return 1;
  }

  if (pg.get<bool>("version"))
  {
    std::cout << pg.name() << " v" << pg.version() << "\n";

    return 1;
  }

  if (pg.get<bool>("license"))
  {
    std::cout << R"(MIT License

Copyright (c) 2019 Brett Robinson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.)" << "\n";

    return 1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  Parg pg {argc, argv};
  int pstatus {program_options(pg)};
  if (pstatus > 0) return 0;
  if (pstatus < 0) return 1;

  try
  {
    Tui tui;

    if (! OB::Term::is_term(STDOUT_FILENO))
    {
      throw std::runtime_error("stdout is not a tty");
    }

    if (! OB::Term::is_term(STDIN_FILENO))
    {
      // read from stdin
      tui.init("*stdin*");

      // reset stdin
      int tty = open("/dev/tty", O_RDONLY);
      dup2(tty, STDIN_FILENO);
      close(tty);
    }
    else if (auto const file = pg.get_pos_vec(); ! file.empty())
    {
      // read from file
      tui.init(file.at(0));
    }
    else
    {
      // default
      tui.init();
    }

    // load config file
    tui.config(pg.get("config"));

    // start event loop
    tui.run();
  }
  catch(std::exception const& e)
  {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  catch(...)
  {
    std::cerr << "Error: an unexpected error occurred\n";
    return 1;
  }

  return 0;
}
