-- Basic Blaze script example
-- Assign to an object via Components → Add Component → Script

speed = 0
counter = 0

function OnStart()
	print("Script started on", selfName)
	speed = 10
	Log.Info("Hello from Lua variables: speed=" .. tostring(speed))
end

function OnUpdate(dt)
	counter = counter + dt

	if Input.IsKeyDown("A") then
		print("Holding A — speed=", speed, "t=", string.format("%.2f", counter))
	end

	if Input.IsKeyDown("D") then
		print("Holding D")
	end

	if Input.IsKeyPressed("Space") then
		print("Space held —", selfName)
	end

	if Input.IsMouseDown(MOUSE_LEFT) then
		print("LMB at", Input.GetMouseX(), Input.GetMouseY())
	end
end
