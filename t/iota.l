.include eru.l

rev ← (Y λrev.λl.(((null? (cdr l)) ((cons (car l)) nil)) ((append' (rev (cdr l))) (car l))))
append' ← (Y λappend'.λl.λel.(((null? l) ((cons el) nil)) ((cons (car l)) ((append' (cdr l)) el))))
riota ← (Y λriota.λx.(((zero? x) nil) ((cons x) (riota (pred x)))))

output ← (car (rev #(riota 3)))
