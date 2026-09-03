from pygments.style import Style
from pygments.token import Comment, Keyword, Name, Number, Operator, Punctuation, String


class SppSphinxStyle(Style):
    default_style: str
    background_color: str
    highlight_color: str
    styles: dict[str, str]

    def __init__(self) -> None:
        Style.__init__(self)
        self.default_style = ""
        self.background_color = "#1c1c1c"
        self.highlight_color = "#3c3c3c"
        self.styles = {
            Keyword: "#ff6000",
            Number: "#ff6000",
            String: "#00ff00",
            Operator: "#ffff00",
            Punctuation: "#ffff00",
            Name: "#c0c0c0",
            Comment.Singleline: "#9f40ff",
            Comment.Multiline: "#00ff00",
        }


__all__ = ["SppSphinxStyle"]
