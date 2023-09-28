-- Copyright (c) 2023 Marc Balmer HB9SSB
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to
-- deal in the Software without restriction, including without limitation the
-- rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
-- sell copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
-- FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
-- IN THE SOFTWARE.

-- Lower half of the trx-control Lua part

-- Yaesu FT-817 CAT driver

local validModes = {
	lsb = 0x00,
	usb = 0x01,
	cw = 0x02,
	cwr = 0x03,
	am = 0x04,
	wfm = 0x06,
	fm = 0x08,
	dig = 0x0a,
	pkt = 0x0c
}

local function initialize()
	trx.setspeed(38400)
end

local function setFrequency(frequency)
	local freq = string.sub(string.format('%09d', frequency), 1, -2)
	local bcd = trx.stringToBcd(freq)
	trx.write(bcd .. '\x01')
	return frequency
end

local function getFrequency()
	trx.write('\x00\x00\x00\x00\x03')
	local f = trx.read(5)
	local frequency = trx.bcdToString(string.sub(f, 1, 4))
	local modeCode = string.byte(string.sub(f, 5))
	local mode = ''
	for k, v in pairs(validModes) do
		if v == modeCode then
			mode = k
		end
	end
	if #mode == 0 then
		mode = nil
	end
	local fn = tonumber(frequency) * 10
	return fn, mode
end

local function setMode(band, mode)
	print (string.format('ft-817: set mode to %s', mode))
	local newMode = validModes[mode]
	if newMode ~= nil then
		trx.write(string.format('%c\x00\x00\x00\x07', newMode))
		return band, mode
	else
		return band, 'invalid mode ' .. mode
	end
end

local function getMode()
	print 'ft-817: get mode'
	trx.write('\x00\x00\x00\x00\x03')
	local f = trx.read(5)
	local m = string.byte(f, 5)
	for k, v in pairs(validModes) do
		if v == m then
			return k
		end
	end
	return 'unknown mode'
end


return {
	statusUpdatesRequirePolling = true,
	initialize = initialize,
	startStatusUpdates = nil,
	stopStatusUpdates = nil,
	setFrequency = setFrequency,
	getFrequency = getFrequency,
	getMode = getMode,
	setMode = setMode
}
