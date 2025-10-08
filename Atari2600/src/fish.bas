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

 player0x = 16
 player0y = 45

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

 player1x = 150
 player1y = 8

 COLUBK = $9E
 player1y = (rand&63) + 1


main
 if joy0fire then player0y = player0y - 2 : goto control_done
control_done

 if collision(player0, player1) then score = 0 : player0x = 16 : player0y = 45 : h = 0 : b = 1
 if player0y = 100 then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : h = 0 : b = 1
 if player0y = 0 then score = 0 : player0x = 16 : player0y = 45 : player1x = 150 : h = 0 : b = 1
 if player1x = 0 then player1x = 150 : player1y = (rand&63) + 1
 if h = 100 then h = 0 : b = b + 1

 player1x = player1x - b
 h = h + 1
 score = score + b

 player0y = player0y + 1

 COLUP0 = $2A
 COLUP1 = $5C
 
 drawscreen
 
 goto main
