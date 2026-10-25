# dbus-viewer

[![CI](https://github.com/S3lios/dbus-viewer/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/S3lios/dbus-viewer/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/S3lios/dbus-viewer?sort=semver)](https://github.com/S3lios/dbus-viewer/releases/latest)

Interactive terminal monitor for D-Bus messages, written in C++23 on top of
`sd-bus` (libsystemd) and `ncursesw`.

## Build

```sh
sudo apt install build-essential cmake pkg-config libsystemd-dev libncurses-dev
cmake -S . -B build
cmake --build build -j
```

## Usage

```sh
./build/dbus-viewer              # session bus (default)
sudo ./build/dbus-viewer --system
./build/dbus-viewer --address unix:path=/run/user/1000/bus
./build/dbus-viewer -n 50000     # keep 50000 messages in history
# Only signals from one interface, plus calls to one service (rules are OR-ed):
./build/dbus-viewer --match "type='signal',interface='org.freedesktop.Notifications'" \
                    --match "destination='org.gnome.Shell'"
```

Three views, switched with `1`, `2`, `3` or `Tab`:

| View     | Grouping                                                                 |
|----------|--------------------------------------------------------------------------|
| Stream   | None: every message scrolls by; `Enter` shows the full message dump.     |
| Members  | One row per `interface.member`: calls / returns / errors / signals, rate over the last 10 s and average rate. |
| Services | Tree per connection: members it sent, members it received, and the interfaces it exposes (introspected on demand). |

Method returns and errors are attributed to the call they answer, so they are
grouped with it. In the Members and Services views, `Enter` on a member opens
the stream filtered on it.

Other keys: `/` filter, `Esc` clear filter, `p` pause, `c` clear, `s` sort
(Members), `d` hide disconnected (Services), `?` help, `q` quit.

## Tests

```sh
ctest --test-dir build --output-on-failure
```

Unit tests cover option parsing, introspection XML, text layout, rates and
list navigation. Integration tests run under `dbus-run-session`, which starts
a private bus: they check capture, reply attribution and match rules against
a real daemon. Catch2 is downloaded at configure time unless installed;
pass `-DBUILD_TESTING=OFF` to skip the tests entirely.

## Versioning and releases

The version lives in `project(dbus-viewer VERSION x.y.z)` in `CMakeLists.txt`
and is reported by `dbus-viewer --version`.

- CI builds and tests every push and pull request on `main` and `dev`, with
  GCC.
- A pull request to `main` fails unless it raises the version.
- Each push to `main` whose version has no tag yet creates the `vX.Y.Z` tag
  and a GitHub release with the Linux x86-64 binary and generated notes.

## How it works

- A first connection calls `org.freedesktop.DBus.Monitoring.BecomeMonitor`
  and receives a copy of every message on the bus.
- A second, regular connection resolves well-known names (`ListNames`,
  `GetNameOwner`, then `NameOwnerChanged` signals) and introspects services.
  Its own traffic is hidden from the views.
- Introspection only targets object paths a service was seen using, and is
  sent with auto-start disabled so that it never activates a service.

## License

Copyright (C) 2026 Mathurin Mael

dbus-viewer is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [LICENSE](LICENSE) for the full text.
