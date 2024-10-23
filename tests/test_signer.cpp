#include <fstream>
#include <vector>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
using namespace std;
namespace fs = std::filesystem;

std::vector<unsigned char> es25519_signer(const std::vector<unsigned char> &data, const std::string &private_key_path)
{
    if (data.empty())
    {
        throw std::runtime_error("Signature data is empty");
    }

    // Initialize OpenSSL
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    // Load the private key
    FILE *key_file = fopen(private_key_path.c_str(), "r");
    if (!key_file)
    {
        throw std::runtime_error("Failed to open private key file");
    }
    EVP_PKEY *pkey = PEM_read_PrivateKey(key_file, nullptr, nullptr, nullptr);
    fclose(key_file);
    if (!pkey)
    {
        throw std::runtime_error("Failed to read private key");
    }

    // Create and initialize the signing context
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx)
    {
        EVP_PKEY_free(pkey);
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }
    if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) <= 0)
    {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        throw std::runtime_error("Failed to initialize DigestSign");
    }

    // Sign the data
    if (EVP_DigestSignUpdate(ctx, data.data(), data.size()) <= 0)
    {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        throw std::runtime_error("Failed to update DigestSign");
    }

    size_t sig_len = 0;
    if (EVP_DigestSignFinal(ctx, nullptr, &sig_len) <= 0)
    {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        throw std::runtime_error("Failed to finalize DigestSign (size)");
    }

    std::vector<unsigned char> signature(sig_len);
    if (EVP_DigestSignFinal(ctx, signature.data(), &sig_len) <= 0)
    {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        throw std::runtime_error("Failed to finalize DigestSign (signature)");
    }

    // Clean up
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    EVP_cleanup();
    ERR_free_strings();

    return signature;
}

vector<unsigned char> test_signer(const std::vector<unsigned char> &data)
{
    fs::path private_key_path = fs::path(__FILE__).parent_path() / "fixtures/es256_private.key";
    return es25519_signer(data, private_key_path.c_str());
};
