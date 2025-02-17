// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

// Creates an RSA-OAEP algorithm
blink::WebCryptoAlgorithm CreateRsaOaepAlgorithm(
    const std::vector<uint8_t>& label) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdRsaOaep,
      new blink::WebCryptoRsaOaepParams(
          !label.empty(), vector_as_array(&label),
          static_cast<unsigned int>(label.size())));
}

scoped_ptr<base::DictionaryValue> CreatePublicKeyJwkDict() {
  scoped_ptr<base::DictionaryValue> jwk(new base::DictionaryValue());
  jwk->SetString("kty", "RSA");
  jwk->SetString("n",
                 Base64EncodeUrlSafe(HexStringToBytes(kPublicKeyModulusHex)));
  jwk->SetString("e",
                 Base64EncodeUrlSafe(HexStringToBytes(kPublicKeyExponentHex)));
  return jwk.Pass();
}

class WebCryptoRsaOaepTest : public WebCryptoTestBase {};

// Import a PKCS#8 private key that uses RSAPrivateKey with the
// id-rsaEncryption OID.
TEST_F(WebCryptoRsaOaepTest, ImportPkcs8WithRsaEncryption) {
  blink::WebCryptoKey private_key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatPkcs8,
                      CryptoData(HexStringToBytes(kPrivateKeyPkcs8DerHex)),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaOaep,
                          blink::WebCryptoAlgorithmIdSha1),
                      true, blink::WebCryptoKeyUsageDecrypt, &private_key));
}

TEST_F(WebCryptoRsaOaepTest, ImportPublicJwkWithNoAlg) {
  scoped_ptr<base::DictionaryValue> jwk(CreatePublicKeyJwkDict());

  blink::WebCryptoKey public_key;
  ASSERT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(*jwk.get(), CreateRsaHashedImportAlgorithm(
                                           blink::WebCryptoAlgorithmIdRsaOaep,
                                           blink::WebCryptoAlgorithmIdSha1),
                           true, blink::WebCryptoKeyUsageEncrypt, &public_key));
}

TEST_F(WebCryptoRsaOaepTest, ImportPublicJwkWithMatchingAlg) {
  scoped_ptr<base::DictionaryValue> jwk(CreatePublicKeyJwkDict());
  jwk->SetString("alg", "RSA-OAEP");

  blink::WebCryptoKey public_key;
  ASSERT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(*jwk.get(), CreateRsaHashedImportAlgorithm(
                                           blink::WebCryptoAlgorithmIdRsaOaep,
                                           blink::WebCryptoAlgorithmIdSha1),
                           true, blink::WebCryptoKeyUsageEncrypt, &public_key));
}

TEST_F(WebCryptoRsaOaepTest, ImportPublicJwkWithMismatchedAlgFails) {
  scoped_ptr<base::DictionaryValue> jwk(CreatePublicKeyJwkDict());
  jwk->SetString("alg", "RSA-OAEP-512");

  blink::WebCryptoKey public_key;
  ASSERT_EQ(
      Status::ErrorJwkAlgorithmInconsistent(),
      ImportKeyJwkFromDict(*jwk.get(), CreateRsaHashedImportAlgorithm(
                                           blink::WebCryptoAlgorithmIdRsaOaep,
                                           blink::WebCryptoAlgorithmIdSha1),
                           true, blink::WebCryptoKeyUsageEncrypt, &public_key));
}

TEST_F(WebCryptoRsaOaepTest, ImportPublicJwkWithMismatchedTypeFails) {
  scoped_ptr<base::DictionaryValue> jwk(CreatePublicKeyJwkDict());
  jwk->SetString("kty", "oct");
  jwk->SetString("alg", "RSA-OAEP");

  blink::WebCryptoKey public_key;
  ASSERT_EQ(
      Status::ErrorJwkUnexpectedKty("RSA"),
      ImportKeyJwkFromDict(*jwk.get(), CreateRsaHashedImportAlgorithm(
                                           blink::WebCryptoAlgorithmIdRsaOaep,
                                           blink::WebCryptoAlgorithmIdSha1),
                           true, blink::WebCryptoKeyUsageEncrypt, &public_key));
}

TEST_F(WebCryptoRsaOaepTest, ExportPublicJwk) {
  struct TestData {
    blink::WebCryptoAlgorithmId hash_alg;
    const char* expected_jwk_alg;
  } kTestData[] = {{blink::WebCryptoAlgorithmIdSha1, "RSA-OAEP"},
                   {blink::WebCryptoAlgorithmIdSha256, "RSA-OAEP-256"},
                   {blink::WebCryptoAlgorithmIdSha384, "RSA-OAEP-384"},
                   {blink::WebCryptoAlgorithmIdSha512, "RSA-OAEP-512"}};
  for (size_t i = 0; i < arraysize(kTestData); ++i) {
    const TestData& test_data = kTestData[i];
    SCOPED_TRACE(test_data.expected_jwk_alg);

    scoped_ptr<base::DictionaryValue> jwk(CreatePublicKeyJwkDict());
    jwk->SetString("alg", test_data.expected_jwk_alg);

    // Import the key in a known-good format
    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::Success(),
              ImportKeyJwkFromDict(
                  *jwk.get(),
                  CreateRsaHashedImportAlgorithm(
                      blink::WebCryptoAlgorithmIdRsaOaep, test_data.hash_alg),
                  true, blink::WebCryptoKeyUsageEncrypt, &public_key));

    // Now export the key as JWK and verify its contents
    std::vector<uint8_t> jwk_data;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatJwk, public_key, &jwk_data));
    EXPECT_TRUE(VerifyPublicJwk(jwk_data, test_data.expected_jwk_alg,
                                kPublicKeyModulusHex, kPublicKeyExponentHex,
                                blink::WebCryptoKeyUsageEncrypt));
  }
}

TEST_F(WebCryptoRsaOaepTest, EncryptDecryptKnownAnswerTest) {
  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("rsa_oaep.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);

    base::DictionaryValue* test = NULL;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    blink::WebCryptoAlgorithm digest_algorithm =
        GetDigestAlgorithm(test, "hash");
    ASSERT_FALSE(digest_algorithm.isNull());
    std::vector<uint8_t> public_key_der =
        GetBytesFromHexString(test, "public_key");
    std::vector<uint8_t> private_key_der =
        GetBytesFromHexString(test, "private_key");
    std::vector<uint8_t> ciphertext = GetBytesFromHexString(test, "ciphertext");
    std::vector<uint8_t> plaintext = GetBytesFromHexString(test, "plaintext");
    std::vector<uint8_t> label = GetBytesFromHexString(test, "label");

    blink::WebCryptoAlgorithm import_algorithm = CreateRsaHashedImportAlgorithm(
        blink::WebCryptoAlgorithmIdRsaOaep, digest_algorithm.id());
    blink::WebCryptoKey public_key;
    blink::WebCryptoKey private_key;

    ASSERT_NO_FATAL_FAILURE(ImportRsaKeyPair(
        public_key_der, private_key_der, import_algorithm, false,
        blink::WebCryptoKeyUsageEncrypt, blink::WebCryptoKeyUsageDecrypt,
        &public_key, &private_key));

    blink::WebCryptoAlgorithm op_algorithm = CreateRsaOaepAlgorithm(label);
    std::vector<uint8_t> decrypted_data;
    ASSERT_EQ(Status::Success(),
              Decrypt(op_algorithm, private_key, CryptoData(ciphertext),
                      &decrypted_data));
    EXPECT_BYTES_EQ(plaintext, decrypted_data);
    std::vector<uint8_t> encrypted_data;
    ASSERT_EQ(Status::Success(),
              Encrypt(op_algorithm, public_key, CryptoData(plaintext),
                      &encrypted_data));
    std::vector<uint8_t> redecrypted_data;
    ASSERT_EQ(Status::Success(),
              Decrypt(op_algorithm, private_key, CryptoData(encrypted_data),
                      &redecrypted_data));
    EXPECT_BYTES_EQ(plaintext, redecrypted_data);
  }
}

TEST_F(WebCryptoRsaOaepTest, EncryptWithLargeMessageFails) {
  const blink::WebCryptoAlgorithmId kHash = blink::WebCryptoAlgorithmIdSha1;
  const size_t kHashSize = 20;

  scoped_ptr<base::DictionaryValue> jwk(CreatePublicKeyJwkDict());

  blink::WebCryptoKey public_key;
  ASSERT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                *jwk.get(), CreateRsaHashedImportAlgorithm(
                                blink::WebCryptoAlgorithmIdRsaOaep, kHash),
                true, blink::WebCryptoKeyUsageEncrypt, &public_key));

  // The maximum size of an encrypted message is:
  //   modulus length
  //   - 1 (leading octet)
  //   - hash size (maskedSeed)
  //   - hash size (lHash portion of maskedDB)
  //   - 1 (at least one octet for the padding string)
  size_t kMaxMessageSize = (kModulusLengthBits / 8) - 2 - (2 * kHashSize);

  // The label has no influence on the maximum message size. For simplicity,
  // use the empty string.
  std::vector<uint8_t> label;
  blink::WebCryptoAlgorithm op_algorithm = CreateRsaOaepAlgorithm(label);

  // Test that a message just before the boundary succeeds.
  std::string large_message;
  large_message.resize(kMaxMessageSize - 1, 'A');

  std::vector<uint8_t> ciphertext;
  ASSERT_EQ(Status::Success(), Encrypt(op_algorithm, public_key,
                                       CryptoData(large_message), &ciphertext));

  // Test that a message at the boundary succeeds.
  large_message.resize(kMaxMessageSize, 'A');
  ciphertext.clear();

  ASSERT_EQ(Status::Success(), Encrypt(op_algorithm, public_key,
                                       CryptoData(large_message), &ciphertext));

  // Test that a message greater than the largest size fails.
  large_message.resize(kMaxMessageSize + 1, 'A');
  ciphertext.clear();

  ASSERT_EQ(Status::OperationError(),
            Encrypt(op_algorithm, public_key, CryptoData(large_message),
                    &ciphertext));
}

// Ensures that if the selected hash algorithm for the RSA-OAEP message is too
// large, then it is rejected, independent of the actual message to be
// encrypted.
// For example, a 1024-bit RSA key is too small to accomodate a message that
// uses OAEP with SHA-512, since it requires 1040 bits to encode
// (2 * hash size + 2 padding bytes).
TEST_F(WebCryptoRsaOaepTest, EncryptWithLargeDigestFails) {
  const blink::WebCryptoAlgorithmId kHash = blink::WebCryptoAlgorithmIdSha512;

  scoped_ptr<base::DictionaryValue> jwk(CreatePublicKeyJwkDict());

  blink::WebCryptoKey public_key;
  ASSERT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                *jwk.get(), CreateRsaHashedImportAlgorithm(
                                blink::WebCryptoAlgorithmIdRsaOaep, kHash),
                true, blink::WebCryptoKeyUsageEncrypt, &public_key));

  // The label has no influence on the maximum message size. For simplicity,
  // use the empty string.
  std::vector<uint8_t> label;
  blink::WebCryptoAlgorithm op_algorithm = CreateRsaOaepAlgorithm(label);

  std::string small_message("A");
  std::vector<uint8_t> ciphertext;
  // This is an operation error, as the internal consistency checking of the
  // algorithm parameters is up to the implementation.
  ASSERT_EQ(Status::OperationError(),
            Encrypt(op_algorithm, public_key, CryptoData(small_message),
                    &ciphertext));
}

TEST_F(WebCryptoRsaOaepTest, DecryptWithLargeMessageFails) {
  blink::WebCryptoKey private_key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatPkcs8,
                      CryptoData(HexStringToBytes(kPrivateKeyPkcs8DerHex)),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaOaep,
                          blink::WebCryptoAlgorithmIdSha1),
                      true, blink::WebCryptoKeyUsageDecrypt, &private_key));

  // The label has no influence on the maximum message size. For simplicity,
  // use the empty string.
  std::vector<uint8_t> label;
  blink::WebCryptoAlgorithm op_algorithm = CreateRsaOaepAlgorithm(label);

  std::string large_dummy_message(kModulusLengthBits / 8, 'A');
  std::vector<uint8_t> plaintext;

  ASSERT_EQ(Status::OperationError(),
            Decrypt(op_algorithm, private_key, CryptoData(large_dummy_message),
                    &plaintext));
}

TEST_F(WebCryptoRsaOaepTest, WrapUnwrapRawKey) {
  blink::WebCryptoAlgorithm import_algorithm = CreateRsaHashedImportAlgorithm(
      blink::WebCryptoAlgorithmIdRsaOaep, blink::WebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  ASSERT_NO_FATAL_FAILURE(ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex), import_algorithm, false,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageWrapKey,
      blink::WebCryptoKeyUsageDecrypt | blink::WebCryptoKeyUsageUnwrapKey,
      &public_key, &private_key));

  std::vector<uint8_t> label;
  blink::WebCryptoAlgorithm wrapping_algorithm = CreateRsaOaepAlgorithm(label);

  const std::string key_hex = "000102030405060708090A0B0C0D0E0F";
  const blink::WebCryptoAlgorithm key_algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);

  blink::WebCryptoKey key =
      ImportSecretKeyFromRaw(HexStringToBytes(key_hex), key_algorithm,
                             blink::WebCryptoKeyUsageEncrypt);
  ASSERT_FALSE(key.isNull());

  std::vector<uint8_t> wrapped_key;
  ASSERT_EQ(Status::Success(),
            WrapKey(blink::WebCryptoKeyFormatRaw, key, public_key,
                    wrapping_algorithm, &wrapped_key));

  // Verify that |wrapped_key| can be decrypted and yields the key data.
  // Because |private_key| supports both decrypt and unwrap, this is valid.
  std::vector<uint8_t> decrypted_key;
  ASSERT_EQ(Status::Success(),
            Decrypt(wrapping_algorithm, private_key, CryptoData(wrapped_key),
                    &decrypted_key));
  EXPECT_BYTES_EQ_HEX(key_hex, decrypted_key);

  // Now attempt to unwrap the key, which should also decrypt the data.
  blink::WebCryptoKey unwrapped_key;
  ASSERT_EQ(Status::Success(),
            UnwrapKey(blink::WebCryptoKeyFormatRaw, CryptoData(wrapped_key),
                      private_key, wrapping_algorithm, key_algorithm, true,
                      blink::WebCryptoKeyUsageEncrypt, &unwrapped_key));
  ASSERT_FALSE(unwrapped_key.isNull());

  std::vector<uint8_t> raw_key;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatRaw, unwrapped_key, &raw_key));
  EXPECT_BYTES_EQ_HEX(key_hex, raw_key);
}

TEST_F(WebCryptoRsaOaepTest, WrapUnwrapJwkSymKey) {
  // The public and private portions of a 2048-bit RSA key with the
  // id-rsaEncryption OID
  const char kPublicKey2048SpkiDerHex[] =
      "30820122300d06092a864886f70d01010105000382010f003082010a0282010100c5d8ce"
      "137a38168c8ab70229cfa5accc640567159750a312ce2e7d54b6e2fdd59b300c6a6c9764"
      "f8de6f00519cdb90111453d273a967462786480621f9e7cee5b73d63358448e7183a3a68"
      "e991186359f26aa88fbca5f53e673e502e4c5a2ba5068aeba60c9d0c44d872458d1b1e2f"
      "7f339f986076d516e93dc750f0b7680b6f5f02bc0d5590495be04c4ae59d34ba17bc5d08"
      "a93c75cfda2828f4a55b153af912038438276cb4a14f8116ca94db0ea9893652d02fc606"
      "36f19975e3d79a4d8ea8bfed6f8e0a24b63d243b08ea70a086ad56dd6341d733711c89ca"
      "749d4a80b3e6ecd2f8e53731eadeac2ea77788ee55d7b4b47c0f2523fbd61b557c16615d"
      "5d0203010001";
  const char kPrivateKey2048Pkcs8DerHex[] =
      "308204bd020100300d06092a864886f70d0101010500048204a7308204a3020100028201"
      "0100c5d8ce137a38168c8ab70229cfa5accc640567159750a312ce2e7d54b6e2fdd59b30"
      "0c6a6c9764f8de6f00519cdb90111453d273a967462786480621f9e7cee5b73d63358448"
      "e7183a3a68e991186359f26aa88fbca5f53e673e502e4c5a2ba5068aeba60c9d0c44d872"
      "458d1b1e2f7f339f986076d516e93dc750f0b7680b6f5f02bc0d5590495be04c4ae59d34"
      "ba17bc5d08a93c75cfda2828f4a55b153af912038438276cb4a14f8116ca94db0ea98936"
      "52d02fc60636f19975e3d79a4d8ea8bfed6f8e0a24b63d243b08ea70a086ad56dd6341d7"
      "33711c89ca749d4a80b3e6ecd2f8e53731eadeac2ea77788ee55d7b4b47c0f2523fbd61b"
      "557c16615d5d02030100010282010074b70feb41a0b0fcbc207670400556c9450042ede3"
      "d4383fb1ce8f3558a6d4641d26dd4c333fa4db842d2b9cf9d2354d3e16ad027a9f682d8c"
      "f4145a1ad97b9edcd8a41c402bd9d8db10f62f43df854cdccbbb2100834f083f53ed6d42"
      "b1b729a59072b004a4e945fc027db15e9c121d1251464d320d4774d5732df6b3dbf751f4"
      "9b19c9db201e19989c883bbaad5333db47f64f6f7a95b8d4936b10d945aa3f794cfaab62"
      "e7d47686129358914f3b8085f03698a650ab5b8c7e45813f2b0515ec05b6e5195b6a7c2a"
      "0d36969745f431ded4fd059f6aa361a4649541016d356297362b778e90f077d48815b339"
      "ec6f43aba345df93e67fcb6c2cb5b4544e9be902818100e9c90abe5f9f32468c5b6d630c"
      "54a4d7d75e29a72cf792f21e242aac78fd7995c42dfd4ae871d2619ff7096cb05baa78e3"
      "23ecab338401a8059adf7a0d8be3b21edc9a9c82c5605634a2ec81ec053271721351868a"
      "4c2e50c689d7cef94e31ff23658af5843366e2b289c5bf81d72756a7b93487dd8770d69c"
      "1f4e089d6d89f302818100d8a58a727c4e209132afd9933b98c89aca862a01cc0be74133"
      "bee517909e5c379e526895ac4af11780c1fe91194c777c9670b6423f0f5a32fd7691a622"
      "113eef4bed2ef863363a335fd55b0e75088c582437237d7f3ed3f0a643950237bc6e6277"
      "ccd0d0a1b4170aa1047aa7ffa7c8c54be10e8c7327ae2e0885663963817f6f02818100e5"
      "aed9ba4d71b7502e6748a1ce247ecb7bd10c352d6d9256031cdf3c11a65e44b0b7ca2945"
      "134671195af84c6b3bb3d10ebf65ae916f38bd5dbc59a0ad1c69b8beaf57cb3a8335f19b"
      "c7117b576987b48331cd9fd3d1a293436b7bb5e1a35c6560de4b5688ea834367cb0997eb"
      "b578f59ed4cb724c47dba94d3b484c1876dcd70281807f15bc7d2406007cac2b138a96af"
      "2d1e00276b84da593132c253fcb73212732dfd25824c2a615bc3d9b7f2c8d2fa542d3562"
      "b0c7738e61eeff580a6056239fb367ea9e5efe73d4f846033602e90c36a78db6fa8ea792"
      "0769675ec58e237bd994d189c8045a96f5dd3a4f12547257ce224e3c9af830a4da3c0eab"
      "9227a0035ae9028180067caea877e0b23090fc689322b71fbcce63d6596e66ab5fcdbaa0"
      "0d49e93aba8effb4518c2da637f209028401a68f344865b4956b032c69acde51d29177ca"
      "3db99fdbf5e74848ed4fa7bdfc2ebb60e2aaa5354770a763e1399ab7a2099762d525fea0"
      "37f3e1972c45a477e66db95c9609bb27f862700ef93379930786cf751b";
  blink::WebCryptoAlgorithm import_algorithm = CreateRsaHashedImportAlgorithm(
      blink::WebCryptoAlgorithmIdRsaOaep, blink::WebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  ASSERT_NO_FATAL_FAILURE(ImportRsaKeyPair(
      HexStringToBytes(kPublicKey2048SpkiDerHex),
      HexStringToBytes(kPrivateKey2048Pkcs8DerHex), import_algorithm, false,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageWrapKey,
      blink::WebCryptoKeyUsageDecrypt | blink::WebCryptoKeyUsageUnwrapKey,
      &public_key, &private_key));

  std::vector<uint8_t> label;
  blink::WebCryptoAlgorithm wrapping_algorithm = CreateRsaOaepAlgorithm(label);

  const std::string key_hex = "000102030405060708090a0b0c0d0e0f";
  const blink::WebCryptoAlgorithm key_algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);

  blink::WebCryptoKey key =
      ImportSecretKeyFromRaw(HexStringToBytes(key_hex), key_algorithm,
                             blink::WebCryptoKeyUsageEncrypt);
  ASSERT_FALSE(key.isNull());

  std::vector<uint8_t> wrapped_key;
  ASSERT_EQ(Status::Success(),
            WrapKey(blink::WebCryptoKeyFormatJwk, key, public_key,
                    wrapping_algorithm, &wrapped_key));

  // Verify that |wrapped_key| can be decrypted and yields a valid JWK object.
  // Because |private_key| supports both decrypt and unwrap, this is valid.
  std::vector<uint8_t> decrypted_jwk;
  ASSERT_EQ(Status::Success(),
            Decrypt(wrapping_algorithm, private_key, CryptoData(wrapped_key),
                    &decrypted_jwk));
  EXPECT_TRUE(VerifySecretJwk(decrypted_jwk, "A128CBC", key_hex,
                              blink::WebCryptoKeyUsageEncrypt));

  // Now attempt to unwrap the key, which should also decrypt the data.
  blink::WebCryptoKey unwrapped_key;
  ASSERT_EQ(Status::Success(),
            UnwrapKey(blink::WebCryptoKeyFormatJwk, CryptoData(wrapped_key),
                      private_key, wrapping_algorithm, key_algorithm, true,
                      blink::WebCryptoKeyUsageEncrypt, &unwrapped_key));
  ASSERT_FALSE(unwrapped_key.isNull());

  std::vector<uint8_t> raw_key;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatRaw, unwrapped_key, &raw_key));
  EXPECT_BYTES_EQ_HEX(key_hex, raw_key);
}

TEST_F(WebCryptoRsaOaepTest, ImportExportJwkRsaPublicKey) {
  struct TestCase {
    const blink::WebCryptoAlgorithmId hash;
    const blink::WebCryptoKeyUsageMask usage;
    const char* const jwk_alg;
  };
  const TestCase kTests[] = {{blink::WebCryptoAlgorithmIdSha1,
                              blink::WebCryptoKeyUsageEncrypt,
                              "RSA-OAEP"},
                             {blink::WebCryptoAlgorithmIdSha256,
                              blink::WebCryptoKeyUsageEncrypt,
                              "RSA-OAEP-256"},
                             {blink::WebCryptoAlgorithmIdSha384,
                              blink::WebCryptoKeyUsageEncrypt,
                              "RSA-OAEP-384"},
                             {blink::WebCryptoAlgorithmIdSha512,
                              blink::WebCryptoKeyUsageEncrypt,
                              "RSA-OAEP-512"}};

  for (size_t test_index = 0; test_index < arraysize(kTests); ++test_index) {
    SCOPED_TRACE(test_index);
    const TestCase& test = kTests[test_index];

    const blink::WebCryptoAlgorithm import_algorithm =
        CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaOaep,
                                       test.hash);

    // Import the spki to create a public key
    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::WebCryptoKeyFormatSpki,
                        CryptoData(HexStringToBytes(kPublicKeySpkiDerHex)),
                        import_algorithm, true, test.usage, &public_key));

    // Export the public key as JWK and verify its contents
    std::vector<uint8_t> jwk;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatJwk, public_key, &jwk));
    EXPECT_TRUE(VerifyPublicJwk(jwk, test.jwk_alg, kPublicKeyModulusHex,
                                kPublicKeyExponentHex, test.usage));

    // Import the JWK back in to create a new key
    blink::WebCryptoKey public_key2;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::WebCryptoKeyFormatJwk, CryptoData(jwk),
                        import_algorithm, true, test.usage, &public_key2));
    ASSERT_TRUE(public_key2.handle());
    EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key2.type());
    EXPECT_TRUE(public_key2.extractable());
    EXPECT_EQ(import_algorithm.id(), public_key2.algorithm().id());

    // TODO(eroman): Export the SPKI and verify matches.
  }
}

}  // namespace

}  // namespace webcrypto
