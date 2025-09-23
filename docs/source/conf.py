import os
import sys

from pygments.lexer import RegexLexer
from pygments.token import Keyword, Name, String, Number, Operator, Punctuation, Text, Comment
from sphinx.application import Sphinx

sys.path.insert(0, os.path.abspath("_static/style"))

# -- Project information -----------------------------------------------------

project = 's++'
copyright = '2025, Sam Gardner'
author = 'Sam Gardner'
release = '0.1.0'

# -- General configuration ---------------------------------------------------

templates_path = ['_templates']
exclude_patterns = []

extensions = [
    "breathe"
]

breathe_projects = {
    "s++": "../build/doxygen/xml"
}

breathe_default_project = "s++"

# -- Options for HTML output -------------------------------------------------
html_theme = "furo"
html_theme_options = {
    "dark_css_variables": {
        "color-api-name": "#ffff00",
        "color-api-pre-name": "#ff3300",
        "color-brand-primary": "#FF643F",
        "color-brand-content": "#FF643F",
        "color-brand-visited": "#FF643F",
    }
}
html_static_path = ["_static"]
html_css_files = [
    "css/custom.css"
]


# -- Custom lexer class ------------------------------------------------------

class SppSphinxLexer(RegexLexer):
    name = "S++"
    aliases = ["spp", "s++"]
    filenames = ["*.spp"]

    tokens = {
        "root": [
            (r"\b(cls|fun|cor|sup|ext|mut|use|cmp|let|type|self|Self|case|of|loop|iter|in|else|gen|ret|exit|skip|is|as|or|and|not|async|true|false|res|caps|iter)\b", Keyword),
            (r"\b\d+\b", Number),
            (r"\"[^\"]*\"", String),
            (r"[\+\-\*/%=\?<>&!]", Operator),
            (r"[\(\)\{\}\[\]:,@\.(->)]", Punctuation),
            (r"\b[a-zA-Z_][a-zA-Z0-9_]*\b", Name),
            (r"\s+", Text),
            (r"#.*$", Comment.Singleline),
            (r"(##).*?(##)", Comment.Multiline),
        ],
    }


pygments_dark_style = "custom.SppSphinxStyle"


# -- Modify the setup --------------------------------------------------------
# def setup(app: Sphinx) -> None:
#     app.add_lexer("S++", SppSphinxLexer)
