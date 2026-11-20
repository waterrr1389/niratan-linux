# Third-party components

This program is GPL-3.0-or-later. Every component below is compatible with that
license. A new dependency is added here before it is added to the build.

| Component | How it is included | License | Pinned at |
|---|---|---|---|
| hoshidicts | submodule `third_party/hoshidicts`, linked statically | GPL-3.0 | `885c49f` |
| glaze | submodule of hoshidicts | MIT | `12f6116` |
| zstd | submodule of hoshidicts | BSD-3-Clause | `885c79b` |
| libdeflate | submodule of hoshidicts | MIT | `28e88e9` |
| utf8proc | submodule of hoshidicts | MIT (Unicode data terms apply) | `26dbf59` |
| utfcpp | submodule of hoshidicts | BSL-1.0 | `9dd9052` |
| xxHash | submodule of hoshidicts | BSD-2-Clause | `e573d4d` |
| unordered_dense | submodule of hoshidicts | MIT | `7b55cab` |
| kanji-processor | submodule of hoshidicts, embedded as data | MIT | `452cc2d` |
| Qt 6.12 | system library, linked dynamically | LGPL-3.0 | distribution package |
| libmpv | system library, linked dynamically | GPL-2.0-or-later | distribution package |
| libzip | system library, linked dynamically | BSD-3-Clause | distribution package |
| SQLite | system library, linked dynamically | Public Domain | distribution package |

hoshidicts and its submodules keep their own license texts. Qt and libmpv stay
dynamically linked so they can be replaced, which their licenses require.

Web resources ported from Niratan keep their original copyright headers, which
include the Yomitan and Yomichan notices.
