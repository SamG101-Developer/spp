module spp.parse.errors;


spp::parse::errors::SyntacticError::SyntacticError(std::string &&header) :
    header(std::move(header)) {
}


spp::parse::errors::SppSyntaxError::SppSyntaxError(std::string &&header) :
    SyntacticError(std::move(header)),
    pos(0),
    tokens({}) {
}
