module;
#include <spp/macros.hpp>

module spp.utils.types;

SPP_MOD_BEGIN

// template <typename T>
// spp::Unique<T>::~Unique() {
//     delete this->_Ptr;
// }
//
// template <typename T>
// auto spp::Unique<T>::operator=(Unique &&other) noexcept -> Unique& {
//     _Ptr = std::exchange(other._Ptr, nullptr);
//     return *this;
// }
//
// template <typename T>
// template <typename U> requires std::derived_from<U, T>
// spp::Unique<T>::Unique(Unique<U> &&other) noexcept {
//     _Ptr = std::exchange(other._Ptr, nullptr);
//     return *this;
// }
//
// template <typename T>
// auto spp::Unique<T>::operator=(std::nullptr_t) noexcept -> Unique& {
//     Reset();
//     return *this;
// }
//
//
// spp::Unique::

SPP_MOD_END
