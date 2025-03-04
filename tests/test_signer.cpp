#include <fstream>
#include <vector>
#include "c2pa.hpp"
#include "unit_test.h"
using namespace std;
namespace fs = std::filesystem;

vector<unsigned char> test_signer(const std::vector<unsigned char> &data)
{
    // inline private key for ed25519 certificate
    std::string private_key = R"(-----BEGIN PRIVATE KEY-----
MC4CAQAwBQYDK2VwBCIEIL2+9INLPNSLH3STzKQJ3Wen9R6uPbIYOIKA2574YQ4O
-----END PRIVATE KEY-----)";
    auto signature = c2pa_ed25519_sign(data.data(), data.size(), private_key.c_str());
    auto result = vector<unsigned char>(signature, signature + data.size());
    c2pa_signature_free(signature);
    return result;
};

