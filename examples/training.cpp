// Copyright 2023 Adobe. All rights reserved.
// This file is licensed to you under the Apache License,
// Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
// or the MIT license (http://opensource.org/licenses/MIT),
// at your option.
// Unless required by applicable law or agreed to in writing,
// this software is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR REPRESENTATIONS OF ANY KIND, either express or
// implied. See the LICENSE-MIT and LICENSE-APACHE files for the
// specific language governing permissions and limitations under
// each license.

#include "c2pa.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

// this example uses nlohmann json for parsing the manifest
using json = nlohmann::json;
using namespace std;
namespace fs = std::filesystem;

/// @brief The manifest JSON to be signed
/// @details This is a simple manifest that contains a single assertion
/// that indicates that the image was not used for training or mining
/// @details The manifest is a JSON object with the following structure:
/// - claim_version: The version of the claim (set to 2 for v2 claims)
/// - claim_generator_info: Information about the generator of the claim
/// - assertions: An array of assertions
const json manifest_json = {
  {"claim_version", 2},
  {"claim_generator_info", {
      {{"name", "c2pa-c test"}, {"version", "0.2"}}
  }},
  {"assertions", {
      {
          {"label", "cawg.training-mining"},
          {"data", {
              {"entries", {
                  {"cawg.ai_inference", {{"use", "notAllowed"}}},
                  {"cawg.ai_generative_training", {{"use", "notAllowed"}}}
              }}
          }}
      }
  }}
};

/// @brief Read a text file into a string
string read_text_file(const fs::path &path)
{
  ifstream file(path);
  if (!file.is_open())
  {
    throw runtime_error("Could not open file " + path.string());
  }
  string contents((istreambuf_iterator<char>(file)),
                  istreambuf_iterator<char>());
  file.close();
  return contents.data();
}

/// @brief Get a test signer
/// @return A signer for testing
c2pa::Signer get_signer()
{
  fs::path project_root = fs::path(__FILE__).parent_path().parent_path();

  string certs =
      read_text_file(project_root / "tests/fixtures/es256_certs.pem").data();
  string p_key =
      read_text_file(project_root / "tests/fixtures/es256_private.key").data();
  return c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");
}

/// @brief Example of signing a file with a manifest and reading the manifest
/// back
/// @details This shows how to write a do not train assertion and read the
/// status back
/// @return 0 on success, 1 on failure
int main()
{
  // Get the root directory of the project
  fs::path project_root = fs::path(__FILE__).parent_path().parent_path();

  // Construct the paths relative to the project root
  fs::path image_path = project_root / "tests/fixtures/A.jpg";
  fs::path output_path = project_root / "target/example/training.jpg";

  cout << "The C2pa library version is " << c2pa::version() << endl;
  cout << "RUNNING EXAMPLE training.cpp " << endl;

  try
  {
    // create a signer
    c2pa::Signer signer = get_signer();

    auto builder = c2pa::Builder(manifest_json.dump());
    auto manifest_data = builder.sign(image_path, output_path, signer);

    // read the new manifest and display the JSON
    auto reader = c2pa::Reader(output_path);
    auto new_manifest_json = reader.json();
    cout << "The new manifest is " << new_manifest_json << endl;

    // parse the manifest and display the AI training status
    bool allowed = true; // default to allowed
    json manifest_store = json::parse(new_manifest_json);

    // get the active manifest
    string active_manifest = manifest_store["active_manifest"];
    json &manifest = manifest_store["manifests"][active_manifest];

    // scan the assertions for the training-mining assertion
    for (auto &assertion : manifest["assertions"])
    {
      if (assertion["label"] == "c2pa.training-mining")
      {
        for (json &entry : assertion["data"]["entries"])
        {
          if (entry["use"] == "notAllowed")
          {
            allowed = false;
          }
        }
      }
    }
    cout << "AI training is " << (allowed ? "allowed" : "not allowed") << endl;
  }
  catch (c2pa::C2paException const &e)
  {
    cerr << "C2PA Error: " << e.what() << endl;
  }
  catch (const std::exception &error)
  {
    std::cerr << "Error: " << error.what() << std::endl;
  }
  catch (...)
  {
    cerr << "An unknown error occurred." << endl;
  }
}
