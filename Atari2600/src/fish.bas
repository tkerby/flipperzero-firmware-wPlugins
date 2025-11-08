 m = 1
 
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

main
 if player0y > player1y - 8 && player0y < player1y + 8 then player0y = player0y - 2
 if player0y >= 90 then  player0y = player0y - 2
 if collision(player0, player1) then score = 0 : player0x = 16 : player0y = 45 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0 : m = 1
 if player0y = 100 then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : h = 0 : b = 1 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0 : m = 1
 if player0y = 0 then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : h = 0 : b = 1 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0 : m = 1
 if player1x <= 16 then player1x = 150 : player1y = (rand&110) + 10
 if a < 100 then a = a + 1
 if a = 100 then b = b + 1
 if b = 100 then c = c + 1 : a = 0
 if c = 100 then d = d + 1 : b = 0
 if d = 100 then e = e + 1 : c = 0
 if e = 100 then m = m + 1 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0

 player1x = player1x - m

 score = score + m

 player0y = player0y + 1
 
 if !switchleftb then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : h = 0 : b = 1 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0 : m = 1 : goto single
 if switchreset then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : h = 0 : b = 1 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0 : m = 1 : goto main
 if !switchbw then COLUBK = $9E : COLUP0 = $2A : COLUP1 = $5C
 if switchbw then COLUBK = $02 : COLUP0 = $0C : COLUP1 = $0C
 if !switchrightb then COLUBK = $2A : COLUP0 = $00 : COLUP1 = $00

 drawscreen
 
 goto main

single
 if joy0fire then player0y = player0y - 2 : goto single_done
single_done

 if collision(player0, player1) then score = 0 : player0x = 16 : player0y = 45 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0 : m = 1
 if player0y = 100 then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : h = 0 : b = 1 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0 : m = 1
 if player0y = 0 then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : h = 0 : b = 1 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0 : m = 1
 if player1x <= 16 then player1x = 150 : player1y = (rand&110) + 10
 if a < 100 then a = a + 1
 if a = 100 then b = b + 1
 if b = 100 then c = c + 1 : a = 0
 if c = 100 then d = d + 1 : b = 0
 if d = 100 then e = e + 1 : c = 0
 if e = 100 then m = m + 1 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0

 player1x = player1x - m

 score = score + m

 player0y = player0y + 1

 if switchleftb then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : h = 0 : b = 1 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0 : m = 1 : goto main
 if switchreset then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : h = 0 : b = 1 : a = 0 : b = 0 : c = 0 : d = 0 : e = 0 : m = 1 : goto main
 if !switchbw then COLUBK = $9E : COLUP0 = $2A : COLUP1 = $5C
 if switchbw then COLUBK = $02 : COLUP0 = $0C : COLUP1 = $0C
 if !switchrightb then COLUBK = $2A : COLUP0 = $00 : COLUP1 = $00
 
 drawscreen
 
 goto single
