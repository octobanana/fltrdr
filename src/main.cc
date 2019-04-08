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

#include <filesystem>
namespace fs = std::filesystem;

// prototypes
int program_options(Parg& pg);

int program_options(Parg& pg)
{
  pg.name("fltrdr").version("0.2.1 (28.03.2019)");
  pg.description("A TUI text reader for the terminal.");

  pg.usage("[--config-base <dir>] [--config|-u <file>] [<file>]");
  pg.usage("[--help|-h]");
  pg.usage("[--version|-v]");
  pg.usage("[--license]");

  pg.info("Mouse Bindings", {
    "middle click\n    toggle view",
    "right click\n    toggle play/pause",
    "scroll up\n    goto previous word",
    "scroll down\n    goto next word",
  });

  pg.info("Prompt Bindings", {
    "<esc>|<ctrl-c>\n    exit the prompt",
    "<enter>\n    submit the input",
    "<ctrl-u>\n    clear the prompt",
    "<up>|<ctrl-p>\n    previous history value based on current input",
    "<down>|<ctrl-n>\n    next history value based on current input",
    "<left>|<ctrl-b>\n    move cursor left",
    "<right>|<ctrl-f>\n    move cursor right",
    "<home>|<ctrl-a>\n    move cursor to the start of the input",
    "<end>|<ctrl-e>\n    move cursor to the end of the input",
    "<delete>|<ctrl-d>\n    delete character under the cursor or delete previous character if cursor is at the end of the input",
    "<backspace>|<ctrl-h>\n    delete previous character",
  });

  pg.info("Key Bindings", {
    "q|Q|<ctrl-c>\n    quit the program",
    "w\n    save state",
    ":\n    enter the command prompt",
    "/\n    search forwards prompt",
    "?\n    search backwards prompt",
    "<esc>\n    pause or discard prompt",
    "<space>\n    toggle play/pause",
    "gg\n    goto beginning",
    "G\n    goto end",
    "v\n    toggle view",
    "i\n    increase show previous word",
    "I\n    decrease show previous word",
    "o\n    increase show next word",
    "O\n    decrease show next word",
    "*\n    search next current word",
    "#\n    search previous current word",
    "n\n    goto next search result",
    "N\n    goto previous search result",
    "h|<left>\n    goto previous word",
    "l|<right>\n    goto next word",
    "H\n    goto previous sentence",
    "L\n    goto next sentence",
    "j|<down>\n    decrease wpm",
    "k|<up>\n    increase wpm",
    "J\n    goto previous chapter",
    "K\n    goto next chapter",
  });

  pg.info("Commands", {
    "q|quit|Quit|exit\n    quit the program",
    "w\n    save state",
    "wq\n    save state and quit the program",
    "open <path>\n    open file for reading",
    "wpm <60-1200>\n    set wpm value",
    "goto <\\d+>\n    goto specified word index",
    "prev <0-60>\n    set number of previous words to show",
    "next <0-60>\n    set number of next words to show",
    "offset <0-6>\n    set offset of focus point from center",

    R"RAW(
  timer <value>
    clear
      clear the timer
    <\d\dY:\d\dM:\d\dW:\d\dD:\d\dh:\d\dm:\d\ds>
      set the timer value)RAW",

    R"RAW(
  wpm-avg <value>
    clear
      clear the wpm average
    <\d+>
      set the wpm average value)RAW",

    R"RAW(
  set <value> <on|off>
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
      toggle border bottom visibility)RAW",

    R"RAW(
  sym <value> <char>
    progress
      progress bar and fill symbol
    progress-bar
      progress bar symbol
    progress-fill
      progress bar fill symbol
    border
      border top/bottom line and border top/bottom mark symbol
    border-top
      border top line and border top mark symbol
    border-top-mark
      border top mark symbol
    border-top-line
      border top line symbol
    border-bottom
      border bottom line and border bottom mark symbol
    border-bottom-mark
      border bottom mark symbol
    border-bottom-line
      border bottom line symbol)RAW",

    R"RAW(
  style <value> <#000000-#ffffff|0-255|Colour|reverse|clear>
    primary
      meta style, sets the primary colour of the program
    secondary
      meta style, sets the secondary colour of the program
    text
      meta style, sets the colours of all input text shown in the reader
    background
      set the background colour
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
    progress-bar
      set the progress bar colour
    progress-fill
      set progress bar fill colour
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
    R"RAW(Base Config Directory (BASE): '${HOME}/.fltrdr'
  State Directory: 'BASE/state'
  History Directory: 'BASE/history'
  Config File: 'BASE/config'
  State Files: 'BASE/state/<content-id>'
  Search History File: 'BASE/history/search'
  Command History File: 'BASE/history/command'

  Use '--config=<file>' to override the default config file.
  Use '--config-base=<dir>' to override the default base config directory.

  The base config directory and config file must be created by the user.
  The config file in the base config directory must be named 'config'.
  It is a plain text file that can contain any of the
  commands listed in the 'Commands' section of the '--help' output.
  Each command must be on its own line. Lines that begin with the
  '#' character are treated as comments.)RAW"
  });

  pg.info("Examples", {
    "fltrdr",
    "fltrdr <file>",
    "cat <file> | fltrdr",
    "fltrdr --config \"./path/to/config/file\"",
    "fltrdr --config-base \"~/.config/fltrdr\"",
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
  pg.set("help,h", "Print the help output.");
  pg.set("version,v", "Print the program version.");
  pg.set("license", "Print the program license.");

  // options
  pg.set("config,u", "", "file", "Use the commands in the config file 'file' for initialization.\n    All other initializations are skipped. To skip all initializations,\n    use the special name 'NONE'.");
  pg.set("config-base", "", "dir", "use 'dir' as the base config directory.\n    To skip all initializations,\n    use the special name 'NONE'.");

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

  std::ios_base::sync_with_stdio(false);

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

    // load files
    {
      // determine base config directory
      // default to '~/.fltrdr'
      fs::path base_config_dir {pg.find("config-base") ?
        pg.get<fs::path>("config-base") :
        fs::path(OB::Term::env_var("HOME") + "/." + pg.name())};

      if (base_config_dir != "NONE" &&
        fs::exists(base_config_dir) && fs::is_directory(base_config_dir))
      {
        // set base config directory
        tui.base_config(base_config_dir);

        // check/create default directories

        fs::path state_dir {base_config_dir / fs::path("state")};

        if (! fs::exists(state_dir) || ! fs::is_directory(state_dir))
        {
          fs::create_directory(state_dir);
        }

        fs::path history_dir {base_config_dir / fs::path("history")};

        if (! fs::exists(history_dir) || ! fs::is_directory(history_dir))
        {
          fs::create_directory(history_dir);
        }

        // load history files
        tui.load_hist_command(history_dir / fs::path("command"));
        tui.load_hist_search(history_dir / fs::path("search"));

        // load config file
        tui.load_config(pg.find("config") ? pg.get<fs::path>("config") :
          base_config_dir / fs::path("config"));

        // load content state if available
        tui.load_state();
      }
    }

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
