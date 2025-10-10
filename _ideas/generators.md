# Generators

## Ambiguous

Currently, if a type is iterated and there is more than one inherited generator type, it is an error. This is to prevent
ambiguity. However, if there is a return known, then the generator may be selectable using return type overloading
resolution rules.