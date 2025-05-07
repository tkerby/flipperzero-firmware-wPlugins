# FlipWorld
Open-world multiplayer game for the Flipper Zero, Video Game Module, and other Raspberry Pi Pico devices.

## Connect Online
- Discord: https://discord.gg/5aN9qwkEc6  
- YouTube: https://www.youtube.com/@jblanked  
- Instagram: https://www.instagram.com/jblanked  
- Other: https://www.jblanked.com/social/

## How It Works

**Player Attributes**
- **Health**: The player's life points.  
- **XP**: The player's experience points.  
- **Level**: The player's rank.  
- **Strength**: The player's attack power.  
- **Health Regeneration**: Health regained per second.  
- **Attack Timer**: Cooldown time between attacks.

New players start with 100 health, 0 XP, 10 strength, 1 health regeneration, a 1-second attack timer, and level 1. Each level grants +1 strength and +10 health. XP required to level up increases by a factor of 1.5 each time. For example:
- Level 2: 100 XP  
- Level 3: 150 XP  
- Level 4: 225 XP  

**Enemies**
Enemies have similar attributes but lack XP and health regeneration. A level 1 enemy has 100 health and 10 strength, matching a level 1 player.

**Attacks**
- When an enemy attacks, you lose health equal to its strength.  
- If an enemy defeats you, you also lose XP equal to its strength.  
- When you attack an enemy, you gain health equal to 10% of the enemy’s strength and XP equal to its full strength.

An enemy attack lands if the enemy faces you and collides with you. To attack an enemy, it must face away from you, and you must collide with it while clicking the attack button.

**NPCs**
NPCs are friendly characters with whom players can interact by clicking the attack button while colliding with them.
