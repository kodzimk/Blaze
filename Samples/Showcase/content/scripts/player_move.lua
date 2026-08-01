-- Platformer player: A/D move, Space jump, collect coins, avoid spikes
moveSpeed = 400
jumpSpeed = 760
groundHits = 0
grounded = false

coins = 0
totalCoins = 0
spawnX = 0
spawnY = 0
won = false
hud = nil

function refreshHud()
	if not hud or not hud:IsValid() then
		hud = FindByName("HUD Coins")
	end
	if hud then
		SetText(hud, "Coins: " .. tostring(coins) .. " / " .. tostring(totalCoins))
	end
end

function OnStart()
	groundHits = 0
	grounded = false
	coins = 0
	won = false
	local x, y = GetWorldLocation()
	spawnX, spawnY = x, y

	local list = FindAllByTag("Coin")
	totalCoins = list and #list or 0
	refreshHud()
	print("Collect all coins and reach the GOAL flag!")
end

function OnUpdate(dt)
	if won then
		SetVelocity(0, 0)
		return
	end

	local vx, vy = GetVelocity()
	local move = 0
	if Input.IsKeyDown("A") or Input.IsKeyDown("Left") then
		move = move - 1
	end
	if Input.IsKeyDown("D") or Input.IsKeyDown("Right") then
		move = move + 1
	end
	vx = move * moveSpeed

	if grounded and Input.IsKeyPressed("Space") then
		vy = jumpSpeed
		grounded = false
	end

	-- Fell off the map
	local _, y = GetWorldLocation()
	if y < -400 then
		SetLocation(spawnX, spawnY)
		SetVelocity(0, 0)
		PrintString("Ouch — try again!", 1.5)
	end

	SetVelocity(vx, vy)
end

function OnCollisionBegin(other)
	if other and other:IsValid() then
		groundHits = groundHits + 1
		grounded = true
	end
end

function OnCollisionEnd(other)
	groundHits = math.max(groundHits - 1, 0)
	grounded = groundHits > 0
end

function OnTriggerBegin(other)
	if not other or not other:IsValid() or won then
		return
	end

	if other:HasTag("Coin") then
		coins = coins + 1
		Destroy(other)
		refreshHud()
		PrintString("Coin! (" .. tostring(coins) .. "/" .. tostring(totalCoins) .. ")", 1.2)
		if coins >= totalCoins and totalCoins > 0 then
			PrintString("All coins! Reach the GOAL flag.", 3.0)
		end
		return
	end

	if other:HasTag("Hazard") then
		SetLocation(spawnX, spawnY)
		SetVelocity(0, 0)
		PrintString("Spikes! Back to start.", 2.0)
		return
	end

	if other:HasTag("Goal") then
		if coins >= totalCoins and totalCoins > 0 then
			won = true
			PrintString("YOU WIN! Coins: " .. tostring(coins) .. "/" .. tostring(totalCoins), 8.0)
			print("Showcase complete — you collected every coin.")
		else
			PrintString("Need more coins (" .. tostring(coins) .. "/" .. tostring(totalCoins) .. ")", 2.5)
		end
	end
end
