# LUA s6ftrig - Handle S6 Service Events

This library allows to register to one or more S6 service fifodirs.
The current status can be retrieved and for state changes can be poll()ed.
The queries return a dictionary with the service paths as keys
and a string of one or more event characters.

## Example
```
local ftrig = require "s6ftrig"
local posix = require "posix"

local trig = ftrig.init{ "/service/lighttpd", "/service/dropbear2" }

local function printtrig(trig)
  for k,v in pairs(trig) do
    print(string.format("%s: %s", k, v))
  end
end

printtrig(trig:current())
posix.poll({ [trig:fd()] = { events = { IN=true } } }, -1)
printtrig(trig:wait())
```

## s6ftrig API

- `state(string)` translates the states d D u U s O x to
  finish, down, up, ready, start, once, exit

- `init(array of service dirs)` creates an ftrigr object, listening
  for events on the "/event" fifodirs for all provided service directories.

## ftrigr API

- `fd()` returns the file descriptor to poll() for new events.
- `wait()` returns new state information or an empty array
   if there are no new events.
- `current` returns current service state information by
  calling `s6_svstatus_read()` on each service directory..

The sequence:

 - t = init()
 - t:current()
 - posix.poll({ [t:fd()] = { events = { IN=true } } }, -1)
 - t:wait()

is race free.
