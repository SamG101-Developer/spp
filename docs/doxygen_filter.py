"""
Doxygen INPUT_FILTER: normalises doc comments on the way into doxygen, without touching the source tree.

Doxygen treats "@code" as a block command that runs until a delimited "@endcode". A large number of docstrings in this
project use it inline with no separating whitespace (eg "@code (@endcode"), which doxygen does not terminate. The lexer
then swallows the rest of the file and every entity below that point is silently dropped from the XML, so whole classes
disappear from the generated API docs.

It also strips the module declarations and import statements. Doxygen does not model these as declarations, so it falls
back to reading them as file-scope variables named "module", "import" and so on, which then show up as documented
entities. Nothing in the API docs describes a module's imports, so they are blanked before doxygen ever sees them.

Doxygen invokes this with a single filename argument and reads the rewritten file from stdout.

Todo: In the future, the codebase docs will be fixed so this isn't needed.
"""

import re
import sys

# An "@code"/"\code" whose "@endcode" is glued to the preceding token.
INLINE_CODE = re.compile(
    r"[@\\]code(?:\{[^}]*\})?[ \t\n]"
    r"(?P<body>[^\n]*?(?:\n[ \t]*\*?[ \t]*[^\n]*?)?)"
    r"(?<!\s)[@\\]endcode"
)

# Comment continuation: a newline plus the leading " * " of the next doc-comment line.
CONTINUATION = re.compile(r"\s*\n[ \t]*\*?[ \t]*")

# Filter off the module-unit preamble: "module;", "export module spp.asts.ast;", "import std;", "export import x;".
MODULE_PREAMBLE = re.compile(
    r"(?m)^[ \t]*(?:module[ \t]*;|(?:export[ \t]+)?(?:module|import)[ \t]+[^;{}]*;)[ \t]*$"
)


def _replace(match: re.Match[str]) -> str:
    body = CONTINUATION.sub(" ", match.group("body")).strip()

    # A body that still contains an opener means the comment was already malformed; leave it for Doxygen to report
    # rather than papering over it with a wrong rewrite.
    if "@code" in body or "\\code" in body:
        return match.group(0)

    # Markdown inline code, which is what these single-token uses were always meant to be. Backticks in the body would
    # terminate it early, so those fall back to Doxygen's own inline command.
    return f"@c {body}" if "`" in body else f"`{body}`"


def _rewrite(text: str) -> str:
    text = INLINE_CODE.sub(_replace, text)

    # Blanked rather than deleted, so every line below keeps its original number and Doxygen's diagnostics still point
    # at the right place in the real source file.
    return MODULE_PREAMBLE.sub("", text)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <file>", file=sys.stderr)
        return 2

    # Doxygen only consumes stdout. Encoding errors are replaced rather than fatal: a filter that dies takes the whole
    # Doxygen run down with it.
    with open(sys.argv[1], encoding="utf-8", errors="replace") as f:
        sys.stdout.write(_rewrite(f.read()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
