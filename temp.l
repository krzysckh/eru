I ← λx.x

true ← λx.λy.x
false ← λx.λy.y

0 ← λf.λx.x
succ ← λn.λf.λx.(f ((n f) x))
+ ← λm.λn.((m succ) n)
- ← λm.λn.((n pred) m)
* ← λm.λn.((m (+ n)) 0)

pred ← λn.λf.λx.(((n (λg.λh.h (g f))) λu.x) I)
zero? ← λn.(n (λx.false true))

;; fac' ← λf.λn.(((zero? n) 1) ((* n) (f (pred n))))
;; Y ← λg.(λx.(g (x x)) λx.(g (x x)))
;; theta ← (λx.λy.(x (x y)) λx.λy.(y (x (x y))))

1 ← (succ 0)
2 ← (succ 1)
3 ← (succ 2)
4 ← (succ 3)

;; i have not managed to get fix combinators working properly yet
;; that's probably because of the naive-ity of my expander :(

output ← ((* 4) 4)
