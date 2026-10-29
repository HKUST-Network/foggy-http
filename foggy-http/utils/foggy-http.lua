-- Load foggy-tcp.lua first. This extension reassembles the lab stream
-- before handing complete messages to Wireshark's native HTTP dissector.
local foggy = Proto("foggyhttp", "HTTP over FoggyTCP")
local seq_field = ProtoField.uint32("foggyhttp.seq", "Stream sequence")
local len_field = ProtoField.uint32("foggyhttp.payload_length", "Payload length")
local msg_field = ProtoField.string("foggyhttp.message", "Reassembled HTTP message")
foggy.fields = {seq_field, len_field, msg_field}
local flows, frames = {}, {}
local transport = Dissector.get("foggytcp")
local http = Dissector.get("http")
function foggy.init() flows = {}; frames = {} end

local function message_length(s)
    if not (s:match("^GET ") or s:match("^POST ") or s:match("^HTTP/1%.[01] ")) then return nil end
    local header_end = s:find("\r\n\r\n", 1, true)
    if not header_end then return nil end
    local body_at = header_end + 4
    local headers = s:sub(1, header_end + 1):lower()
    if headers:find("\r\ntransfer%-encoding:%s*chunked\r\n") then
        local at = body_at
        while true do
            local e = s:find("\r\n", at, true)
            if not e then return nil end
            local hex = s:sub(at, e - 1):match("^([%da-fA-F]+)")
            if not hex then return nil end
            local n = tonumber(hex, 16)
            at = e + 2
            if n == 0 then
                while true do
                    local trailer_end = s:find("\r\n", at, true)
                    if not trailer_end then return nil end
                    if trailer_end == at then return trailer_end + 1 end
                    at = trailer_end + 2
                end
            end
            if #s < at + n + 1 then return nil end
            if s:sub(at + n, at + n + 1) ~= "\r\n" then return nil end
            at = at + n + 2
        end
    end
    local n = tonumber(headers:match("\r\ncontent%-length:%s*(%d+)\r\n")) or 0
    local last = body_at + n - 1
    if #s < last then return nil end
    return last
end

function foggy.dissector(tvb, pinfo, tree)
    if tvb:len() < 25 or tvb(0, 4):uint() ~= 3120 then return 0 end
    transport:call(tvb, pinfo, tree)
    local hlen, plen = tvb(16, 2):uint(), tvb(18, 2):uint()
    if hlen < 25 or plen < hlen or plen > tvb:len() then return tvb:len() end
    local node = tree:add(foggy, tvb())
    local seq = tvb(8, 4):uint()
    node:add(seq_field, seq)
    node:add(len_field, plen - hlen)
    local frame = tonumber(pinfo.number)
    if not pinfo.visited then
        frames[frame] = {}
        local key = tostring(pinfo.src)..":"..tostring(pinfo.src_port)..">"..
                    tostring(pinfo.dst)..":"..tostring(pinfo.dst_port)
        local flow = flows[key]
        if not flow then flow = {next = 0, pending = {}, text = ""}; flows[key] = flow end
        -- Per-byte storage handles retransmitted and overlapping segments in
        -- these small traces. A fresh capture is required per lab session.
        local payload = tvb(hlen, plen - hlen):raw()
        for i = 1, #payload do
            local pos = seq + i - 1
            if pos >= flow.next then flow.pending[pos] = payload:sub(i, i) end
        end
        local added = {}
        while flow.pending[flow.next] do
            added[#added + 1] = flow.pending[flow.next]
            flow.pending[flow.next] = nil
            flow.next = flow.next + 1
        end
        flow.text = flow.text .. table.concat(added)
        while true do
            local n = message_length(flow.text)
            if not n then break end
            frames[frame][#frames[frame] + 1] = flow.text:sub(1, n)
            flow.text = flow.text:sub(n + 1)
        end
    end
    for _, message in ipairs(frames[frame] or {}) do
        node:add(msg_field, message)
        local reconstructed = ByteArray.new(message, true):tvb("Reassembled FoggyTCP HTTP")
        http:call(reconstructed, pinfo, tree)
    end
    return tvb:len()
end
for _, port in ipairs({18081, 18082, 19081, 19082}) do
    DissectorTable.get("udp.port"):add(port, foggy)
end
