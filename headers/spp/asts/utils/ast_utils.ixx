module;
#include <spp/macros.hpp>

export module spp.asts.utils.ast_utils;
import spp.asts.ast;
import std;


namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct TypeAst;

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone(std::unique_ptr<T> const &ast) -> std::unique_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return std::unique_ptr<std::remove_cvref_t<T>>(dynamic_cast<std::remove_cvref_t<T>*>(ast->clone().release()));
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone(std::shared_ptr<T> const &ast) -> std::unique_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return std::unique_ptr<std::remove_cvref_t<T>>(dynamic_cast<std::remove_cvref_t<T>*>(ast->clone().release()));
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone(T *ast) -> std::unique_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return std::unique_ptr<std::remove_cvref_t<T>>(dynamic_cast<std::remove_cvref_t<T>*>(ast->clone().release()));
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone_vec(std::vector<std::unique_ptr<T>> const &asts) -> std::vector<std::unique_ptr<T>> {
        std::vector<std::unique_ptr<T>> cloned_asts;
        cloned_asts.reserve(asts.size());
        for (auto const &x : asts) {
            cloned_asts.emplace_back(ast_clone(x));
        }
        return cloned_asts;
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone_vec_shared(std::vector<std::shared_ptr<T>> const &asts) -> std::vector<std::shared_ptr<T>> {
        std::vector<std::shared_ptr<T>> cloned_asts;
        cloned_asts.reserve(asts.size());
        for (auto const &x : asts) {
            cloned_asts.emplace_back(x);
        }
        return cloned_asts;
    }

    // SPP_EXP_FUN template <typename T>
    // SPP_ATTR_HOT
    // SPP_ATTR_ALWAYS_INLINE
    // auto ast_cast(Ast *ast) -> T* {
    //     return dynamic_cast<T*>(ast);
    // }
    //
    // SPP_EXP_FUN template <typename T>
    // SPP_ATTR_HOT
    // SPP_ATTR_ALWAYS_INLINE
    // auto ast_cast(Ast const *ast) -> T const* {
    //     return dynamic_cast<T const*>(ast);
    // }
    //
    // SPP_EXP_FUN template <typename T, typename U>
    // SPP_ATTR_ALWAYS_INLINE
    // auto ast_cast(std::unique_ptr<U> &&ast) -> std::unique_ptr<T> {
    //     return std::unique_ptr<T>(ast_cast<T>(ast.release()));
    // }

    SPP_EXP_FUN auto ast_name(Ast *ast) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto ast_body(Ast *ast) -> std::vector<Ast*>;
}
