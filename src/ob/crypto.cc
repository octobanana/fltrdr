#include "ob/crypto.hh"

#include <openssl/sha.h>

#include <array>
#include <string>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <optional>

namespace OB::Crypto
{

std::optional<std::string> sha256(std::string_view const str)
{
  SHA256_CTX ctx;

  if (! SHA256_Init(&ctx)) return {};
  if (! SHA256_Update(&ctx, str.data(), str.size())) return {};

  std::array<unsigned char, SHA256_DIGEST_LENGTH> digest;

  if (! SHA256_Final(digest.data(), &ctx)) return {};

  std::ostringstream res;
  res << std::hex << std::setfill('0');

  for (auto const& e : digest)
  {
    res << std::setw(2) << static_cast<int>(e);
  }

  return res.str();
}

} // namespace OB::Crypto
