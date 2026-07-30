-- Basic move + jump (A/D or arrows, Space to jump)
-- Setup on the player object:
--   1. Components → Add Collision → type PhysicsRigidbody2D
--   2. Assign this script
--   3. Give floors Collision (Block) and optionally tag "Ground"
-- Tip: parent Main Camera under the player so the view follows

moveSpeed = 420
jumpSpeed = 700
grounded = false
groundHits = 0

function OnStart()
	grounded = false
	groundHits = 0
	print("Player ready:", selfName)
end

function OnUpdate(dt)
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

	SetVelocity(vx, vy)
end

function OnCollisionBegin(other)
	-- Treat solid contacts as ground (skips triggers)
	if other and other:IsValid() then
		groundHits = groundHits + 1
		grounded = true
	end
end

function OnCollisionEnd(other)
	groundHits = math.max(groundHits - 1, 0)
	grounded = groundHits > 0
end
