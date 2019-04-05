#ifndef OB_CRYPTO_HH
#define OB_CRYPTO_HH

#include <string>
#include <string_view>
#include <optional>

namespace OB::Crypto
{

std::optional<std::string> sha256(std::string_view const str);

} // namespace OB::Crypto

#endif // OB_CRYPTO_HH
