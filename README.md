# Fltrdr
A TUI text reader for the terminal.

![default](https://raw.githubusercontent.com/octobanana/fltrdr/master/assets/default.png)

## About
__Fltrdr__, or *flat-reader*, is an interactive text reader for the terminal.
It is *flat* in the sense that the reader is word-based. It creates a
horizontal stream of words, ignoring all newline characters and reducing extra whitespace.
Its purpose is to facilitate reading, scanning, and searching text.
The program has a play mode that moves the reader forward one word at a time,
along with a configurable words per minute (WPM), setting.

### Features
* word-based text reader
* *vi* inspired key-bindings
* read text from a file or stdin
* play mode with variable speed controlled through WPM
* in play mode, longer pauses occur on words that end in punctuation
* focus point to align the current word
* focus point tries to be smart about its placement,
  ignoring any surrounding punctuation or plurality
* search forwards and backwards using regular expressions
* command prompt to interact with the program
* store your settings in a configuration file
* choose how many words are shown at a time
* colours can be customized
* chars/symbols can be customized
* show/hide parts of the interface

## Usage
View the usage and help output with the `--help` or `-h` flag.
The help output also contains the documentation for the key bindings
and commands for customization.

## Config File
The default config locations are `${XDG_CONFIG_HOME}/fltrdr/config` and `${HOME}/.fltrdr/config`.
A custom path can also be passed to override the default locations using the `--config` option.
The config directory and file must be created by the user.
If the file does not exist, the program continues as normal.

The file, `config`, is a plain text file that can contain any of the commands
listed in the __Commands__ section of the `--help` output.
Each command must be on its own line.
Lines that begin with the `#` character are treated as comments.

An example config file can be found in `./example/config`

## Terminal Compatibility
This program uses raw terminal control sequences to manipulate the terminal,
such as moving the cursor, styling the output text, and clearing the screen.
Although some of the control sequences used may not work as intended on all terminals,
they should work fine on any modern terminal emulator.

## Pre-Build
This section describes what environments this program may run on,
any prior requirements or dependencies needed,
and any third party libraries used.

> #### Important
> Any shell commands using relative paths are expected to be executed in the
> root directory of this repository.

### Environment
* Linux (supported)
* BSD (untested)
* macOS (untested)

### Requirements
* C++17 compiler/library
* CMake >= 3.8

### Dependencies
* stdc++fs (libstdc++fs)

### Libraries:
* [parg](https://github.com/octobanana/parg):
  for parsing CLI args, included as `./src/ob/parg.hh`

## Build
The following shell command will build the project in release mode:
```sh
./build.sh
```
To build in debug mode, run the script with the `--debug` flag.

## Install
The following shell command will install the project in release mode:
```sh
./install.sh
```
To install in debug mode, run the script with the `--debug` flag.

## Config
The following shell commands will create a config directory in the
`XDG_CONFIG_HOME` default directory and copy over the example config file:
```sh
mkdir -p ~/.config/fltrdr
cp ./example/config ~/.config/fltrdr/config
```

## License
This project is licensed under the MIT License.

Copyright (c) 2019 [Brett Robinson](https://octobanana.com/)

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
SOFTWARE.
