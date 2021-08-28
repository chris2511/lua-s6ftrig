local posix = require "posix"
local ftrig = require "s6ftrig"

local trig = ftrig.init{ "test/svc/sv1", "test/svc/sv2" }

trigfd = trig:fd()
local n = 1

local function printtrig(trig)
  for k,v in pairs(trig) do
    if #v ~= 1 then
      print(string.format("% 4d %s: %s", n, k, v))
    end
    for c in v:gmatch"." do
      print(string.format("% 4d %s: %s:%s", n, k, c, ftrig.state(c)))
    end
  end
  n = n +1
  return n
end

local fds = {
   [trigfd] = {events={IN=true}},
   [1] = {events={IN=true}}
}

printtrig(trig:current())
print("Waiting 10 seconds....")
posix.sleep(10)

while true do
  local n = posix.poll(fds, -1)
  --print("Poll done")
  if fds[trigfd].revents.IN then
    --print("printtrig start")
    if printtrig(trig:wait()) >= 100 then os.exit(0) end
    --print("printtrig end")
  end
  if fds[1].revents.IN then
    print("ENTER and out")
    return
  end
end
