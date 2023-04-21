# Pro Pinball Super-Ultra

Pro Pinball Super-Ultra is a modification / patch for [Pro Pinball Ultra by Barnstorm Games](https://www.pro-pinball.com/).


## Features

* Fix text-rendering bug on macOS

New features can be proposed in [the issue tracker](https://github.com/JayFoxRox/Pro-Pinball-Super-Ultra/issues).


## Compatibility

Pro Pinball Super-Ultra is currently compatible with base-game version:

| Windows         | macOS           | Linux           |
|:---------------:|:---------------:|:---------------:|
| *(Unsupported)* | = 1.2.3         | *(Unsupported)* |

You can find the [base-game download links in the wiki](https://github.com/JayFoxRox/Pro-Pinball-Super-Ultra/wiki/DRM-Free-download).


## Usage

### macOS

One time, remove ASLR from pinball binary, otherwise this will only work in debugger:

```
chmod +x ./change_mach_o_flags.py 
./change_mach_o_flags.py --no-pie /Applications/Pro\ Pinball.app/Contents/MacOS/Pro\ Pinball
```

One time, install build dependencies:

```
brew install freetype
```

Building:

```
mkdir build
cd build
cmake ..
make
```

For lldb:

```
make && lldb -o "env DYLD_INSERT_LIBRARIES=\"`pwd`/libhook.dylib\"" /Applications/Pro\ Pinball.app/Contents/MacOS/Pro\ Pinball q
```

For apitrace (expects apitrace build in `$APITRACE`):

```
make && DYLD_INSERT_LIBRARIES="`pwd`/libhook.dylib" DYLD_FRAMEWORK_PATH="$APITRACE/wrappers" /Applications/Pro\ Pinball.app/Contents/MacOS/Pro\ Pinball
```

For playing:

```
DYLD_INSERT_LIBRARIES="`pwd`/libhook.dylib" /Applications/Pro\ Pinball.app/Contents/MacOS/Pro\ Pinball q
```


## License

**(C) 2020 Jannik Vogel**

Licensed under the MIT License. See LICENSE.md for more information.
