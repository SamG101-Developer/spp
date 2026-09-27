module spp.utils.uid;
import std;

auto spp::utils::Uid()
  -> Str {
  static std::size_t uid_counter = 0;
  return "$" + std::to_string(uid_counter++);
}
