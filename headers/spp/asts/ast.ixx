module;
#include <spp/macros.hpp>

export module spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
}


/**
 * The AST base class is inherited by all other AST classes, provided base functionality, including formatted printing
 * and end position identification.
 */
SPP_EXP_CLS struct spp::asts::Ast {
    explicit Ast();

    virtual ~Ast();

    /**
     * Transpile the S++ code into Rust code, to be interpreted by the Rust compiler.
     * @return The ouptut Rust code.
     */
    virtual auto to_rust() const -> std::string = 0;
};
