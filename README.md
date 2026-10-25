# dbus-viewer

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

## How it works

- A first connection calls `org.freedesktop.DBus.Monitoring.BecomeMonitor`
  and receives a copy of every message on the bus.
- A second, regular connection resolves well-known names (`ListNames`,
  `GetNameOwner`, then `NameOwnerChanged` signals) and introspects services.
  Its own traffic is hidden from the views.
- Introspection only targets object paths a service was seen using, and is
  sent with auto-start disabled so that it never activates a service.
