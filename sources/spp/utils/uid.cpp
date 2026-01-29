module spp.utils.uid;
import spp.asts.ast;


auto spp::utils::generate_uid(asts::Ast const *ast)
    -> std::string {
    static std::size_t uid_counter = 0;
    return "$" + std::to_string(reinterpret_cast<std::uintptr_t>(ast)) + "_" + std::to_string(uid_counter++);
}


auto spp::utils::generate_uid()
    -> std::string {
    static std::size_t uid_counter = 0;
    return "$" + std::to_string(uid_counter++);
}
