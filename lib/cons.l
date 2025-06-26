cons ← λx.λy.λf.((f x) y)
car ← λx.(x true)
cdr ← λx.(x false)
nil ← λx.true
null? ← λl.(l λx.λy.false)
