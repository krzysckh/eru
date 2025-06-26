0 ← λf.λx.x
1 ← (succ 0)
2 ← (succ 1)
3 ← (succ 2)
4 ← (succ 3)

succ ← λn.λf.λx.(f ((n f) x))
+ ← λm.λn.((m succ) n)
- ← λm.λn.((n pred) m)
* ← λm.λn.((m (+ n)) 0)
pred ← λn.λf.λx.(((n λg.λh.(h (g f))) λ_.x) I)

zero? ← λn.((n λx.false) true)

fac' ← λf.λn.(((zero? n) 1) ((* n) (f (pred n))))
