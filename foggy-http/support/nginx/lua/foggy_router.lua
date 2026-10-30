-- Fixed course parser. Called once before selecting a UDP session's upstream.
local M = {}
local bit = require "bit"

local function u16(s, i)
    local a, b = s:byte(i, i + 1)
    return a * 256 + b
end

local function u32(s, i)
    return u16(s, i) * 65536 + u16(s, i + 2)
end

function M.target(packet)
    if not packet or #packet < 25 or u32(packet, 1) ~= 3120 then
        return nil, "invalid FoggyTCP identifier/header"
    end
    local hlen, plen = u16(packet, 17), u16(packet, 19)
    if hlen < 25 or hlen >= plen or plen ~= #packet then
        return nil, "invalid FoggyTCP length or empty payload"
    end
    if u32(packet, 9) ~= 0 or bit.band(packet:byte(21), 10) ~= 0 then
        return nil, "first datagram must contain initial application data"
    end
    local payload = packet:sub(hlen + 1, plen)
    local method, target = payload:match("^([A-Z]+) ([^ ]+) HTTP/1%.[01]\r\n")
    if not method then
        return nil, "complete HTTP request line required in first datagram"
    end
    if method ~= "GET" and method ~= "POST" then
        return nil, "unsupported request method"
    end
    return target:match("^[^?]+")
end

function M.route(routes)
    local target, err = M.target(ngx.var.foggy_packet)
    if not target then return nil, err end
    local backend = routes[target]
    if not backend then return nil, "no route for " .. target end
    ngx.var.foggy_backend = backend
    ngx.log(ngx.INFO, "FOGGY_ROUTE ", target, " -> ", backend)
    return true
end

return M
