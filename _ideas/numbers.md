# Numbers

## Literals

Extend literals to allow for `e+` and `e-` suffixes for scientific notation. This will map directly into Boost's
extended integer and floating point types, as these values are used purely for one-time limit checks (cheap).

## Postfix types

Currently the `f`, `s` and `u` postfixes are used to denote float, signed int and unsigned int types respectively.
Extend this to allow for big number support:

- Postfix `_big` will either use `BigInt` or `BigFloat` depending on the Ast.
- An `IntegerLiteralAst` or `FloatLiteralAst` are the only nodes that can use this postfix.
