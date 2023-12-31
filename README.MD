## Franca programming system

![](https://github.com/dmitry-egorov/franca2/blob/main/docs/screenshots/Screenshot%202023-10-30%20050411.png)

Here's the combination of ideas I want to explore.

### Code representation 

The user sees and edits a representation of the language, its ast, not the text.

- by default the representation still looks like text, but in a more readable form and with special inlays that indicate nesting and other properties (see the screenshot)
- by default it looks like a mix of c, rust and python, 
- the representation can be customized
  - to look like any syntax
  - to look like 'blueprints'
  - to look like ui: color selectors, sliders, panels, etc.
- customizeable on a level of a macro, file, module, program.

### Extensible core language 

Low level core language with optional high-level abstractions on top.

- most language constructs are defined in the language itself (if, return, for, while, etc.)
  - as a consequence, the language is highly extensible
- the core language is very close to a syntax tree
  - lisp-like
  - nesting is unambiguous
  - e.g.: '*[a +[b c]]', which is displayed and edited as 'a * (b + c)'
- the user defines custom node types of the ast
  - a macro is an example of a custom node; it's similar to an inline functions
  - a macro can be turned into a function
- statically typed (maybe even structurally typed, similar to golang)
- no garbage collection in the core

### Code lens

Separating representation and the core language allows to introduce "filters" or code lens

- e.g. toggle displaying types of parameters and variables
- e.g. toggle displaying memory management (passing the allocator, freeing, etc.)
- TODO: others?

### Arbitrary compile-time execution

Do as much as possible at compile time.

- any function can be executed at compile time
  - execute with jit
- can take it further with edit time execution
