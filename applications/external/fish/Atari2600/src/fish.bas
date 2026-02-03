 player0:
 %00000000
 %00001100
 %11011110
 %11111111
 %11111111
 %11011110
 %00001100
 %00000000
end

 player1:
 %01000000
 %10100100
 %00101001
 %01001010
 %01111110
 %11111111
 %11111111
 %01111110
end

 player0x = 16
 player0y = 45
 player1y = 45
 b = 1
 c = 0

main
 if player0y > player1y - 8 && player0y < player1y + 8 then player0y = player0y - 2
 if player0y >= 90 then  player0y = player0y - 2
 if collision(player0, player1) then score = 0 : player0x = 16 : player0y = 45 : b = 1 : c = 0
 if player0y = 100 then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : b = 1 : c = 0
 if player0y = 0 then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : b = 1 : c = 0
 if player1x <= 16 then player1x = 150 : player1y = (rand&75) + 10 : score = score + b : c = c + b
 if c >= 10 then b = b + 1 : c = 0

 player1x = player1x - b
 player0y = player0y + 1

 if switchselect then score = 0 : player0x = 16 : player0y = 45 : b = 1 : c = 0 : goto single
 if !switchbw then COLUBK = $9E : COLUP0 = $2A : COLUP1 = $5C
 if switchbw then COLUBK = $02 : COLUP0 = $0C : COLUP1 = $0C

 drawscreen
 
 goto main

single
 if joy0fire then player0y = player0y - 2 : goto single_done
single_done

 if collision(player0, player1) then score = 0 : player0x = 16 : player0y = 45 : b = 1 : c = 0
 if player0y = 100 then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : b = 1 : c = 0
 if player0y = 0 then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : b = 1 : c = 0
 if player1x <= 16 then player1x = 150 : player1y = (rand&75) + 10 : score = score + b : c = c + b
 if c >= 10 then b = b + 1 : c = 0

 player1x = player1x - b
 player0y = player0y + 1

 if switchselect then score = 0 : player0x = 16 : player0y = 45 : b = 1 : c = 0 : goto main
 if !switchbw then COLUBK = $9E : COLUP0 = $2A : COLUP1 = $5C
 if switchbw then COLUBK = $02 : COLUP0 = $0C : COLUP1 = $0C
 
 drawscreen
 
 goto single
