file flash.x
target remote com1
load flash.x
b somewhere
c
disp/i$pc
si
si
p delay=5
disp count
c
c
p delay=10
c
c
p delay=20
c
c
si
si
si
si

