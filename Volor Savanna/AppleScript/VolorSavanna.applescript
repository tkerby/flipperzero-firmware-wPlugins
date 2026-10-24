on user_name()
	global death_level
	global victory_level
	
	display dialog "What is your name?" default answer "" with title "Volor Savanna"
	
	set userName to text returned of result
	
	set death_level to userName & ", YOU LOSE! THE END!"
	set victory_level to userName & ", YOU WIN! THE END!"
	
	volor_savanna_original()
	
end user_name

on volor_savanna_original()
	global death_level
	global victory_level
	
	set name to text returned of (display dialog death_level default answer "" with title "Volor Savanna")
	
end volor_savanna_original

user_name()